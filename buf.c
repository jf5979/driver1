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

#include "buf.h"

#define READWRITE_BUFSIZE 16
#define DEFAULT_BUFSIZE 256
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
 *
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
        Buffer_Tool.ReadBuf=kmalloc(sizeof(char)*READWRITE_BUFSIZE,GFP_KERNEL);
        Buffer_Tool.WriteBuf=kmalloc(sizeof(char)*READWRITE_BUFSIZE,GFP_KERNEL);
        Le_buffer.Buffer=kmalloc(sizeof(char)*DEFAULT_BUFSIZE,GFP_KERNEL);
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
 *@brief Buf_exit
 * Fonction qui permet de quitter le module kernel
 *
 * */

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
                printk(KERN_ALERT "Buf_open in Write Only : locking writer mode (has %d writer) (%s:%u)\n",Buffer_Tool.numWriter, __FUNCTION__, __LINE__);
            }
            else{
                up(&(Buffer_Tool.SemBuf));
                printk(KERN_ALERT "Driver already in writing mode (has %d writer) (%s:%u)\n",Buffer_Tool.numWriter, __FUNCTION__, __LINE__);
                return -ENOTTY;
            }

            break;
        case O_RDWR :
            down_interruptible(&(Buffer_Tool.SemBuf));
            if(Buffer_Tool.numWriter==0){
                Buffer_Tool.numWriter++;
                Buffer_Tool.numReader++;
                up(&(Buffer_Tool.SemBuf));
                printk(KERN_ALERT"Buf_open in Read Write : locking writer mode  (%s:%u)\n", __FUNCTION__, __LINE__);
            }
            else{
                up(&(Buffer_Tool.SemBuf));
                printk(KERN_ALERT "Driver already in writing mode sadly (%s:%u)\n", __FUNCTION__, __LINE__);
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
 * **/
int Buf_release(struct inode *inode, struct file *filp) {
    switch ((filp->f_flags & O_ACCMODE)){
        case O_RDONLY :
            down_interruptible(&(Buffer_Tool.SemBuf));
            if(Buffer_Tool.numReader>0){
                Buffer_Tool.numReader--;
            }
            up(&(Buffer_Tool.SemBuf));
            break;
        case O_WRONLY :
            down_interruptible(&(Buffer_Tool.SemBuf));
            if(Buffer_Tool.numWriter>0){
                Buffer_Tool.numWriter--;
                printk(KERN_ALERT "Buf was open in Write Only : releasing writer lock (%d writer) (%s:%u)\n",Buffer_Tool.numWriter, __FUNCTION__, __LINE__);
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
            printk(KERN_ALERT "Buf was open in Read Write : releasing writer lock (%d writer) (%s:%u)\n",Buffer_Tool.numWriter, __FUNCTION__, __LINE__);
            break;
        default:
            break;

    }
    return 0;
}

static ssize_t Buf_read(struct file *flip, char __user *ubuf, size_t count, loff_t *f_ops){

        int i=0, j=0,empty=0,nb=0;
        nb=(int) count;
        printk(KERN_ALERT"La valeur de nb est : %d (%s:%u)\n",nb, __FUNCTION__, __LINE__);
        if((flip->f_flags)& O_NONBLOCK){

            if(down_trylock(&(Buffer_Tool.SemBuf))!=0){
                printk(KERN_ALERT"Semaphore indisponible  (%s:%u)\n", __FUNCTION__, __LINE__);
                return -EWOULDBLOCK;
            }
            else{
                printk(KERN_ALERT"Semaphore disponible  (%s:%u)\n", __FUNCTION__, __LINE__);
                while(i<nb){
                    if(BufOut(&Le_buffer,&(Buffer_Tool.ReadBuf[j]))!=0){
                        printk(KERN_ALERT"Couldn't pop all requested data from driver (successuful %d) (%s:%u)\n",i, __FUNCTION__, __LINE__);
                        empty=1;
                    }

                    if(empty==1){
                        copy_to_user((ubuf+(i%READWRITE_BUFSIZE)), (char*)Buffer_Tool.ReadBuf, j);
                    }
                    j++;
                    i++;
                    if((j%READWRITE_BUFSIZE)==0){
                        copy_to_user((ubuf+(i%READWRITE_BUFSIZE)), (char*)Buffer_Tool.ReadBuf, READWRITE_BUFSIZE);
                        j=0;
                    }

                }
                up(&(Buffer_Tool.SemBuf));
            }
        }
        else{
            down_interruptible(&(Buffer_Tool.SemBuf));
            while(i<nb){
                if(BufOut(&Le_buffer,&(Buffer_Tool.ReadBuf[j]))!=0){
                    printk(KERN_ALERT"Couldn't pop all requested data from driver (successuful %d) (%s:%u)\n",i, __FUNCTION__, __LINE__);
                    empty=1;
                }

                if(empty==1){
                    copy_to_user((ubuf+(i%READWRITE_BUFSIZE)), (char*)Buffer_Tool.ReadBuf, j);
                }
                j++;
                i++;
                if((j%READWRITE_BUFSIZE)==0){
                    copy_to_user((ubuf+(i%READWRITE_BUFSIZE)), (char*)Buffer_Tool.ReadBuf, READWRITE_BUFSIZE);
                    j=0;
                }
            }
            up(&(Buffer_Tool.SemBuf));

        }
    return 1;
}
static ssize_t Buf_write (struct file *flip, const char __user *ubuf, size_t count,
        loff_t *f_ops){
        size_t i=0;
        int page_write=0,done=0,nb=0;
        nb= (int) count;
        printk(KERN_ALERT"La valeur de count est : %zu %d (%s:%u)\n",count, nb, __FUNCTION__, __LINE__);
        if((flip->f_flags)& O_NONBLOCK){
            if(down_trylock(&(Buffer_Tool.SemBuf))!=0){
                printk(KERN_ALERT"Semaphore indisponible  (%s:%u)\n", __FUNCTION__, __LINE__);
                return -EWOULDBLOCK;
            }
            else{
                printk(KERN_ALERT"Semaphore disponible  (%s:%u)\n", __FUNCTION__, __LINE__);
                do{
                    if(nb<READWRITE_BUFSIZE){
                        copy_from_user(Buffer_Tool.WriteBuf,ubuf,nb);
                        done=1;
                        printk(KERN_ALERT"Done preparing exit (%s:%u)\n", __FUNCTION__, __LINE__);
                    }
                    else{
                        copy_from_user(Buffer_Tool.WriteBuf,(ubuf+page_write),READWRITE_BUFSIZE);
                        nb=nb-READWRITE_BUFSIZE;
                        page_write=page_write+READWRITE_BUFSIZE;
                        printk(KERN_ALERT"Value of nb %d (%s:%u)\n", nb, __FUNCTION__, __LINE__);
                    }

                    i=0;
                    while(i<nb && i < READWRITE_BUFSIZE){
                        if(BufIn(&Le_buffer,&(Buffer_Tool.WriteBuf[i]))!=0){
                            printk(KERN_ALERT"Couldn't push all requested data in buffer (successuful %zu) (%s:%u)\n",i, __FUNCTION__, __LINE__);

                        }
                        i++;

                    }
                    printk(KERN_ALERT"Wrote a page in buffer (successuful %zu) (%s:%u)\n",i, __FUNCTION__, __LINE__);
                }while(done == 0 && Le_buffer.BufFull==0);
                up(&(Buffer_Tool.SemBuf));
                return 1;
            }
        }
        else{
            printk(KERN_ALERT "Mode pouvant etre bloque (%s:%u)\n", __FUNCTION__, __LINE__);
            down_interruptible(&(Buffer_Tool.SemBuf));
            printk(KERN_ALERT "Semaphore capturer (%s:%u)\n", __FUNCTION__, __LINE__);

            do{
                if((nb-READWRITE_BUFSIZE)<=0){
                    copy_from_user(Buffer_Tool.WriteBuf,ubuf,nb);
                    done=1;
                    printk(KERN_ALERT"Done preparing exit (%s:%u)\n", __FUNCTION__, __LINE__);
                }
                else{
                    copy_from_user(Buffer_Tool.WriteBuf,(ubuf+page_write),READWRITE_BUFSIZE);
                    nb=nb-READWRITE_BUFSIZE;
                    page_write=page_write+READWRITE_BUFSIZE;
                    printk(KERN_ALERT"Value of nb %d (%s:%u)\n", nb, __FUNCTION__, __LINE__);
                }
                i=0;
                while(i<nb && i < READWRITE_BUFSIZE){
                   if(BufIn(&Le_buffer,&(Buffer_Tool.WriteBuf[i]))!=0){
                        printk(KERN_ALERT"Couldn't push all requested data in buffer (successuful %zu) (%s:%u)\n",i, __FUNCTION__, __LINE__);
                        break;
                   }
                   i++;
                }
                printk(KERN_ALERT"Wrote a page in buffer (successuful %zu) (%s:%u)\n",i, __FUNCTION__, __LINE__);
            }while(done == 0 && Le_buffer.BufFull==0);
            up(&(Buffer_Tool.SemBuf));
            return 1;
        }
    printk(KERN_ALERT"Fonction finie, sortons de la fonctions (%s:%u)\n", __FUNCTION__, __LINE__);
    return 1;
}
long Buf_ioctl (struct file *flip, unsigned int cmd, unsigned long arg){
    down_interruptible(&(Buffer_Tool.SemBuf));
    printk(KERN_ALERT "IOCTL value %d %d %d",Le_buffer.InIdx,Buffer_Tool.numReader,Le_buffer.BufSize);
    up(&(Buffer_Tool.SemBuf));
    switch (cmd){
        case BUFF_GETNUMDATA:
            return (long) Le_buffer.InIdx;
        case BUFF_GETNUMREADER:
            return (long) Buffer_Tool.numReader;
        case BUFF_GETBUFSIZE:
            return (long) Le_buffer.BufSize;
        case BUFF_SETBUFSIZE :
            break;
        default:
            return -EINVAL;
    }
    return 0;
}




module_init(Buf_init);
module_exit(Buf_exit);

      	
