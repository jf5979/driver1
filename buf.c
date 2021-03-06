#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>
#include <linux/sched.h>

#include "buf.h"

#define READWRITE_BUFSIZE 16
#define DEFAULT_BUFSIZE 256
#define TEST(X,Y) ((X == 1) ? Y : READWRITE_BUFSIZE)
#define SIZE(F,R,M) ((F>R) ? (F-R) : (M-R+F))

MODULE_LICENSE("Dual BSD/GPL");

int Buf_Var = 0;
module_param(Buf_Var, int, S_IRUGO);

EXPORT_SYMBOL_GPL(Buf_Var);

struct class *Buf_class;

typedef struct BufStruct {
    unsigned int InIdx;
    unsigned int OutIdx;
    unsigned short BufFull;
    unsigned short BufEmpty;
    unsigned int BufSize;
    unsigned short *Buffer;
} Buffer;

typedef struct Buf_Dev {
    unsigned short *ReadBuf;
    unsigned short *WriteBuf;
    struct semaphore SemBuf;
    unsigned short numWriter;
    unsigned short numReader;
    dev_t dev;
    struct cdev cdev;
} BDev;

int BufIn (struct BufStruct *Buf, unsigned short *Data);
int BufOut (struct BufStruct *Buf, unsigned short *Data);
static int Buf_init(void);
static void Buf_exit(void);
int Buf_open (struct inode *inode, struct file *flip);
int Buf_release (struct inode *inode, struct file *flip);
static ssize_t Buf_read(struct file *flip, char __user *ubuf, size_t count, loff_t *f_ops);
static ssize_t Buf_write (struct file *flip, const char __user *ubuf, size_t count, loff_t *f_ops);
long Buf_ioctl (struct file *flip, unsigned int cmd, unsigned long arg);

struct file_operations Buf_fops = {
        .owner = THIS_MODULE,
        .open = Buf_open,
        .release = Buf_release,
        .read = Buf_read,
        .write = Buf_write,
        .unlocked_ioctl = Buf_ioctl,
};

//global structure

BDev Buffer_Tool;
Buffer Le_buffer;
static DECLARE_WAIT_QUEUE_HEAD(reading_queue);
static DECLARE_WAIT_QUEUE_HEAD(writing_queue);

/**
 * @brief BufIn permet l'ajout de donnees dans le buffer circulaire
 * @param Buf Pointeur vers la structure du buffer circulaire
 * @param Data Pointeur vers la donnees a ajouter
 * @return 0 en cas de succes -1 en cas d'echec
 */
int BufIn (struct BufStruct *Buf, unsigned short *Data) {
    if (Buf->BufFull) {
        printk(KERN_ALERT"Buffer Full (%s:%u)\n", __FUNCTION__, __LINE__);
        return -1;
    }
    Buf->BufEmpty = 0;
    Buf->Buffer[Buf->InIdx] = *Data;
    Buf->InIdx = (Buf->InIdx + 1) % Buf->BufSize;
    if (Buf->InIdx == Buf->OutIdx)
        Buf->BufFull = 1;
    return 0;
}
/**
 * @brief BufOut permet le retrait de donnees dans le buffer circulaire
 * @param Buf Pointeur vers la structure du buffer circulaire
 * @param Data Pointeur pour la donnees a retirer
 * @return 0 en cas de succes -1 en cas d'echec
 */
int BufOut (struct BufStruct *Buf, unsigned short *Data) {
    if (Buf->BufEmpty)
        return -1;
    Buf->BufFull = 0;
    *Data = Buf->Buffer[Buf->OutIdx];
    Buf->OutIdx = (Buf->OutIdx + 1) % Buf->BufSize;
    if (Buf->OutIdx == Buf->InIdx)
        Buf->BufEmpty = 1;
    return 0;
}

/**
 * Buf_init
 * @brief Fonction qui permet l'initialisation du module
 * @return Retourne un code d'erreur ou 0 en cas de succes
 *
 */
 static int __init Buf_init (void) {
    int result;

    printk(KERN_ALERT"Buf_init (%s:%u) => Initialising driver\n", __FUNCTION__, __LINE__);
    //Reserve quatre instance mineure du driver si besoin est pour ouverture simultane
    result = alloc_chrdev_region(&(Buffer_Tool.dev), 0, 1, "my_Buf_driver");
    if (result < 0)
        printk(KERN_ALERT"Buf_init ERROR IN alloc_chrdev_region (%s:%s:%u)\n", __FILE__, __FUNCTION__, __LINE__);
    else
    printk(KERN_ALERT"Buf_init : MAJOR = %u MINOR = %u (Buf_Var = %u)\n", MAJOR(Buffer_Tool.dev), MINOR(Buffer_Tool.dev), Buf_Var);

    Buf_class = class_create(THIS_MODULE, "Buf_class");
    device_create(Buf_class, NULL, Buffer_Tool.dev, NULL, "Buf_node");
    cdev_init(&(Buffer_Tool.cdev), &Buf_fops);
    Buffer_Tool.cdev.owner = THIS_MODULE;
    if (cdev_add(&(Buffer_Tool.cdev), Buffer_Tool.dev, 1) < 0)
        printk(KERN_ALERT"Buf_init ERROR IN cdev_add (%s:%s:%u)\n", __FILE__, __FUNCTION__, __LINE__);

    //Initialisation des variables importantes :
        Buffer_Tool.numReader=0;
        Buffer_Tool.numWriter=0;
        Buffer_Tool.ReadBuf=kmalloc(sizeof(unsigned short)*READWRITE_BUFSIZE,GFP_KERNEL);
        Buffer_Tool.WriteBuf=kmalloc(sizeof(unsigned short)*READWRITE_BUFSIZE,GFP_KERNEL);
        Le_buffer.Buffer=kmalloc(sizeof(unsigned short)*DEFAULT_BUFSIZE,GFP_KERNEL);
        Le_buffer.BufEmpty=1;
        Le_buffer.BufFull=0;
        Le_buffer.BufSize=DEFAULT_BUFSIZE;
        Le_buffer.InIdx=0;
        Le_buffer.OutIdx=0;
        sema_init(&(Buffer_Tool.SemBuf),0);
        up(&(Buffer_Tool.SemBuf));
        if(Buffer_Tool.ReadBuf==NULL || Buffer_Tool.ReadBuf==NULL || Le_buffer.Buffer==NULL){
            printk(KERN_ALERT "Couldn't initialise read/write buffer (%s:%s:%u)\n", __FILE__, __FUNCTION__, __LINE__);
            return -ENOTTY;
        }
    return 0;
}

/**
 *@brief Buf_exit Fonction qui permet de quitter le module kernel
 *
 */
 static void __exit Buf_exit (void) {
    down_interruptible(&(Buffer_Tool.SemBuf));
    kfree(Buffer_Tool.ReadBuf);
    kfree(Buffer_Tool.WriteBuf);
    kfree(Le_buffer.Buffer);
    up(&(Buffer_Tool.SemBuf));
    cdev_del(&(Buffer_Tool.cdev));
    unregister_chrdev_region(Buffer_Tool.dev, 1);
    device_destroy (Buf_class, Buffer_Tool.dev);
    class_destroy(Buf_class);

    printk(KERN_ALERT "Buf_exit (%s:%u) => Module unloaded successfully\n", __FUNCTION__, __LINE__);
}

/**
 * @brief Fonction qui permet l'ouverture du driver
 * @param inode Pointeur vers la structure inode qui contient les informations sur le fichier
 * @param filp Pointeur vers la structure file qui contient les drapeaux du fichier
 * @return 0 en cas de succes sinon la valeur negative du code d'erreur
 * **/
int Buf_open(struct inode *inode, struct file *filp) {
    switch ((filp->f_flags & O_ACCMODE)){
        case O_RDONLY :
            down_interruptible(&(Buffer_Tool.SemBuf));
            Buffer_Tool.numReader++;
            up(&(Buffer_Tool.SemBuf));
            break;
        case O_WRONLY :
            down_interruptible(&(Buffer_Tool.SemBuf));
            if(Buffer_Tool.numWriter==0){
                Buffer_Tool.numWriter+=1;
                up(&(Buffer_Tool.SemBuf));
                printk(KERN_WARNING "Buf_open in Write Only : locking writer mode (has %d writer) (%s:%u)\n",Buffer_Tool.numWriter, __FUNCTION__, __LINE__);
            }
            else{
                up(&(Buffer_Tool.SemBuf));
                printk(KERN_WARNING "Driver already in writing mode (has %d writer) (%s:%u)\n",Buffer_Tool.numWriter, __FUNCTION__, __LINE__);
                return -ENOTTY;
            }

            break;
        case O_RDWR :
            down_interruptible(&(Buffer_Tool.SemBuf));
            if(Buffer_Tool.numWriter==0){
                Buffer_Tool.numWriter++;
                Buffer_Tool.numReader++;
                up(&(Buffer_Tool.SemBuf));
                printk(KERN_WARNING"Buf_open in Read Write : locking writer mode  (%s:%u)\n", __FUNCTION__, __LINE__);
            }
            else{
                up(&(Buffer_Tool.SemBuf));
                printk(KERN_WARNING "Driver already in writing mode sadly (%s:%u)\n", __FUNCTION__, __LINE__);
                return -ENOTTY;
            }
            break;
        default:
            break;
    }

    return 0;
}
/**
 * @brief Fonction qui permet la fermeture du driver
 * @param inode Pointeur vers la structure inode qui contient les informations sur le fichier
 * @param filp Pointeur vers la structure file qui contient les drapeaux du fichier
 * @return 0 en cas de succes sinon la valeur negative du code d'erreur
 * **/
int Buf_release(struct inode *inode, struct file *filp) {
    switch ((filp->f_flags & O_ACCMODE)){
        case O_RDONLY :
            down_interruptible(&(Buffer_Tool.SemBuf));
            if(Buffer_Tool.numReader>0){
                Buffer_Tool.numReader--;
            }
            up(&(Buffer_Tool.SemBuf));
            printk(KERN_WARNING "Buf was open in Read : releasing driver (%s:%u)\n", __FUNCTION__, __LINE__);
            break;
        case O_WRONLY :
            down_interruptible(&(Buffer_Tool.SemBuf));
            if(Buffer_Tool.numWriter>0){
                Buffer_Tool.numWriter--;
                printk(KERN_WARNING "Buf was open in Write Only : releasing writer lock (%d writer) (%s:%u)\n",Buffer_Tool.numWriter, __FUNCTION__, __LINE__);
            }
            up(&(Buffer_Tool.SemBuf));
            break;
        case O_RDWR :
            down_interruptible(&(Buffer_Tool.SemBuf));
            if(Buffer_Tool.numWriter>0){
                Buffer_Tool.numWriter--;
            }
            if(Buffer_Tool.numReader>0){
                Buffer_Tool.numReader--;
            }
            up(&(Buffer_Tool.SemBuf));
            printk(KERN_WARNING "Buf was open in Read Write : releasing writer lock (%d writer) (%s:%u)\n",Buffer_Tool.numWriter, __FUNCTION__, __LINE__);
            break;
        default:
            break;

    }
    return 0;
}

/**
 * @brief Buf_read Fonction permettant la lecture de donnes dans le buffer
 * @param flip Pointeur vers la structure file du fichier
 * @param ubuf Pointeur vers le buffer de lecture du user space
 * @param count Nombre de charactere a lire
 * @param f_ops Pointeur vers l'offset que le kernel a attribue au driver
 * @return Valeur negative du code d'erreur ou nombre de charatere lu
 */
static ssize_t Buf_read(struct file *flip, char __user *ubuf, size_t count, loff_t *f_ops){
        int i=0, j=0,nb=0,k=0,l=0,done=0;
        nb=(int) count;
        printk(KERN_ALERT"La valeur de nb est : %d (%s:%u)\n",nb, __FUNCTION__, __LINE__);
        if(down_interruptible(&(Buffer_Tool.SemBuf))!=0)
            return -ERESTARTSYS;
/**
 * Cette boucle while est inspiree par la documentation du kernel linux chapitre 6.2
 * http://www.makelinux.net/ldd3/chp-6-sect-2
 */
        while(Le_buffer.BufEmpty){
            up(&(Buffer_Tool.SemBuf));
            if((flip->f_flags)& O_NONBLOCK){
                return -EAGAIN;
            }
            printk(KERN_WARNING"Aucune donnee, Le processus va dormir (%s:%u)\n", __FUNCTION__, __LINE__);
            if(wait_event_interruptible(reading_queue, ((Le_buffer.BufEmpty) != 1))){
                return -ERESTARTSYS;
            }
            if (down_interruptible(&(Buffer_Tool.SemBuf))){
                return -ERESTARTSYS;
            }
        }
        printk(KERN_WARNING"Donnee disponible, Le processus est reveille (%s:%u)\n", __FUNCTION__, __LINE__);

        do{
            if((nb-i)<=READWRITE_BUFSIZE){
                printk(KERN_ALERT"ALMOST DONE\n");
                done=1;
                for(k=0;k<(nb-i);k++){
                    if(BufOut(&Le_buffer,&(Buffer_Tool.ReadBuf[k]))!=0){
                        if((flip->f_flags)& O_NONBLOCK){
                            for(j=0;j<k;j++){
                                copy_to_user(&(ubuf[i+j]),&(Buffer_Tool.ReadBuf[j]),1);
                                printk(KERN_ALERT"CHARACTER COPIED TO USER : %d %c (%s:%u)\n",(i+j),Buffer_Tool.ReadBuf[j], __FUNCTION__, __LINE__);
                            }
                            up(&(Buffer_Tool.SemBuf));
                            wake_up_interruptible(&(writing_queue));

                            return (ssize_t) (i+k);
                        }
                        else{
                            up(&(Buffer_Tool.SemBuf));
                            printk(KERN_WARNING"Manque de donnees, Le processus va dormir (%s:%u)\n", __FUNCTION__, __LINE__);
                            wake_up_interruptible(&(writing_queue));
                            k--;//Pour eviter de sauter un charatere
                            if(wait_event_interruptible(reading_queue, ((Le_buffer.BufEmpty) != 1))){
                                return -ERESTARTSYS;
                            }
                            printk(KERN_WARNING"Donnee disponible, Le processus est reveille (%s:%u)\n", __FUNCTION__, __LINE__);
                            if (down_interruptible(&(Buffer_Tool.SemBuf))){
                                return -ERESTARTSYS;
                            }
                        }

                    }
                }
            }
            else{
                for(j=0;j<READWRITE_BUFSIZE;j++){
                    if(BufOut(&Le_buffer,&(Buffer_Tool.ReadBuf[j]))!=0){
                        if((flip->f_flags)& O_NONBLOCK){
                            for(l=0;l<j;l++){
                                copy_to_user(&(ubuf[i+l]),&(Buffer_Tool.ReadBuf[l]),1);
                                printk(KERN_ALERT"CHARACTER COPIED TO USER : %d %c (%s:%u)\n",(i+j),Buffer_Tool.ReadBuf[j], __FUNCTION__, __LINE__);
                            }
                            up(&(Buffer_Tool.SemBuf));
                            wake_up_interruptible(&(writing_queue));
                            return (ssize_t) (i+j);
                        }
                        else{
                            up(&(Buffer_Tool.SemBuf));
                            printk(KERN_WARNING"Manque de donnees, Le processus va dormir (%s:%u)\n", __FUNCTION__, __LINE__);
                            wake_up_interruptible(&(writing_queue));
                            j--;//Pour eviter de sauter un charactere
                            if(wait_event_interruptible(reading_queue, ((Le_buffer.BufEmpty) != 1))){
                                return -ERESTARTSYS;
                            }
                            printk(KERN_WARNING"Donnee disponible, Le processus est reveille (%s:%u)\n", __FUNCTION__, __LINE__);
                            if (down_interruptible(&(Buffer_Tool.SemBuf))){
                                return -ERESTARTSYS;
                            }
                        }
                    }

                }

            }
            for(j=0;j<TEST(done,k);j++){
                copy_to_user(&(ubuf[i+j]),&(Buffer_Tool.ReadBuf[j]),1);
                printk(KERN_ALERT"CHARACTER COPIED TO USER : %d %c (%s:%u)\n",(i+j),Buffer_Tool.ReadBuf[j], __FUNCTION__, __LINE__);
            }
            i+=READWRITE_BUFSIZE;
        }while(done == 0);
        up(&(Buffer_Tool.SemBuf));
        wake_up_interruptible(&(writing_queue));
        return count;
}
/**
 * @brief Buf_write Fonction permettant l'ecriture de donnes dans le buffer
 * @param flip Pointeur vers la structure file du fichier
 * @param ubuf Pointeur vers le buffer d'ecriture du user space
 * @param count Nombre de charactere a ecrire
 * @param f_ops Pointeur vers l'offset que le kernel a attribue au driver
 * @return Valeur negative du code d'erreur ou nombre de charatere ecrit
 */
static ssize_t Buf_write (struct file *flip, const char __user *ubuf, size_t count,
        loff_t *f_ops){
        int page_write=0,done=0,nb=0,i=0,j=0;
        nb= (int) count;
        printk(KERN_ALERT"La valeur de count est : %zu %d (%s:%u)\n",count, nb, __FUNCTION__, __LINE__);
        if(down_interruptible(&(Buffer_Tool.SemBuf))!=0)
            return -ERESTARTSYS;

        while(Le_buffer.BufFull){
            up(&(Buffer_Tool.SemBuf));
            if((flip->f_flags)& O_NONBLOCK){
                return -EAGAIN;
            }
            printk(KERN_WARNING"Buffer plein, Le processus va dormir (%s:%u)\n", __FUNCTION__, __LINE__);
            if(wait_event_interruptible(writing_queue, ((Le_buffer.BufFull) != 1))){
                return -ERESTARTSYS;
            }
            if (down_interruptible(&(Buffer_Tool.SemBuf))){
                return -ERESTARTSYS;
            }
        }
        printk(KERN_WARNING"Place disponible, Le processus est reveille (%s:%u)\n", __FUNCTION__, __LINE__);

        do{
            if((nb-READWRITE_BUFSIZE)<=0){
                done=1;
                printk(KERN_ALERT"Done preparing exit (%s:%u)\n", __FUNCTION__, __LINE__);
                for(i=0;i<nb;i++){
                    copy_from_user(&(Buffer_Tool.WriteBuf[i]),&(ubuf[j+i]),1);
                }
            }
            else{
                for(i=0;i<READWRITE_BUFSIZE;i++){
                    copy_from_user(&(Buffer_Tool.WriteBuf[i]),&(ubuf[j+i]),1);
                }
                nb=nb-READWRITE_BUFSIZE;
                page_write=page_write+READWRITE_BUFSIZE;
                printk(KERN_ALERT"Value of nb %d (%s:%u)\n", nb, __FUNCTION__, __LINE__);
            }
            i=0;
            while( i < TEST(done,nb)){
                if(BufIn(&Le_buffer,&(Buffer_Tool.WriteBuf[i]))!=0){
                    if((flip->f_flags)& O_NONBLOCK){
                        printk(KERN_ALERT"Couldn't push all requested data in buffer (successuful %d) (%s:%u)\n",(j+i), __FUNCTION__, __LINE__);
                        up(&(Buffer_Tool.SemBuf));
                        wake_up_interruptible(&(reading_queue));
                        return (ssize_t) (j+i);
                    }
                    else{
                        up(&(Buffer_Tool.SemBuf));
                        printk(KERN_WARNING"Manque de place, Le processus va dormir (%s:%u)\n", __FUNCTION__, __LINE__);
                        wake_up_interruptible(&(reading_queue));
                        if(wait_event_interruptible(writing_queue, ((Le_buffer.BufFull) != 1))){
                            return -ERESTARTSYS;
                        }
                        printk(KERN_WARNING"Place disponible, Le processus est reveille (%s:%u)\n", __FUNCTION__, __LINE__);
                        if (down_interruptible(&(Buffer_Tool.SemBuf))){
                            return -ERESTARTSYS;
                        }
                    }
                }
                else{
                    printk(KERN_ALERT"Charatere ecrit : %d %c (%s:%u)\n",i,Buffer_Tool.WriteBuf[j+i], __FUNCTION__, __LINE__);
                    i++;
                }
            }
            j+=(READWRITE_BUFSIZE);
            printk(KERN_ALERT"Wrote a page in buffer (successuful %d) (%s:%u)\n",i, __FUNCTION__, __LINE__);
        }while(done == 0 && Le_buffer.BufFull==0);
        up(&(Buffer_Tool.SemBuf));
        printk(KERN_ALERT"Fonction finie, sortons de la fonctions (%s:%u)\n", __FUNCTION__, __LINE__);
        wake_up_interruptible(&(reading_queue));
        return count;
}

/**
 * @brief Buf_ioctl Fonction permettant l'obtention et la modification de donnees sur le buffer
 * @param flip Pointeur vers la structure file du driver
 * @param cmd Commande pour la fonction ioctl defini dans buf.h
 * @param arg argument pour la taille du nouveau buffer
 * @return retourne le parametre desirer ou un code d'erreur si la commande est inexistante
 */
long Buf_ioctl (struct file *flip, unsigned int cmd, unsigned long arg){
    int buffer=0,size=0,i=0;
    unsigned short *Buffer_temp;
    switch (cmd){
        case BUFF_GETNUMDATA:
            down_interruptible(&(Buffer_Tool.SemBuf));
            if(Le_buffer.InIdx==Le_buffer.OutIdx){
                if(Le_buffer.BufFull){
                    buffer=Le_buffer.BufSize;
                }
                else{
                    buffer=0;
                }
                up(&(Buffer_Tool.SemBuf));
                return buffer;
            }
            else{
                buffer=SIZE(Le_buffer.InIdx,Le_buffer.OutIdx,Le_buffer.BufSize);
            }
            up(&(Buffer_Tool.SemBuf));
            return (long) buffer;
        case BUFF_GETNUMREADER:
            down_interruptible(&(Buffer_Tool.SemBuf));
            buffer=Buffer_Tool.numReader;
            up(&(Buffer_Tool.SemBuf));
            return (long) buffer ;
        case BUFF_GETBUFSIZE:
            down_interruptible(&(Buffer_Tool.SemBuf));
            buffer=Le_buffer.BufSize;
            up(&(Buffer_Tool.SemBuf));
            return (long) buffer;
        case BUFF_SETBUFSIZE :
            if (!capable(CAP_SYS_ADMIN))
                return -EACCES;
            if(down_trylock(&(Buffer_Tool.SemBuf))!=0){
                return -EWOULDBLOCK;
            }
            if(Le_buffer.InIdx==Le_buffer.OutIdx){
                if(Le_buffer.BufFull){
                    size=Le_buffer.BufSize;
                }
                else{
                   size=0;
                }
            }
            else{
                size=SIZE(Le_buffer.InIdx,Le_buffer.OutIdx,Le_buffer.BufSize);
            }
            if(arg<size){
                up(&(Buffer_Tool.SemBuf));
                return -ENOTTY;
            }

            Buffer_temp=kmalloc(sizeof(unsigned short)*arg,GFP_KERNEL);
            for(i=0;i<size;i++){
                Buffer_temp[i]=Le_buffer.Buffer[(Le_buffer.OutIdx +i)%(Le_buffer.BufSize)];
            }
            kfree(Le_buffer.Buffer);
            Le_buffer.Buffer=Buffer_temp;
            Le_buffer.OutIdx=0;
            if(size==arg){
                Le_buffer.InIdx=0;
                Le_buffer.BufFull=1;
                Le_buffer.BufEmpty=0;
            } else{
                Le_buffer.InIdx=size;
            }
            Le_buffer.BufSize=arg;
            up(&(Buffer_Tool.SemBuf));
            return 0;
        default:
            return -EINVAL;
    }
}

module_init(Buf_init);
module_exit(Buf_exit);

      	
