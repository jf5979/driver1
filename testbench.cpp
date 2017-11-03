//
// Created by jf on 10/24/17.
//


#include <iostream>
#include <thread>         // std::this_thread::sleep_for
#include <chrono>         // std::chrono::seconds
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include "buf.h"

int main(void){
    char saisie_de_user[300];
    char lecture_fichier[256];
    char input=0x00;
    int i=0,nb_char=0,taille=0;
    int fp;




    while(1){
        std::cout << "\033[2J\033[1;1H";
        std::cout<<"1. Ecrire dans le fichier\n2. Lire dans le fichier\n3. Ouvrir fermer le fichier\n4. IOCTL\n";
        std::cin>>input;
        switch (input) {
            case '1' :
                std::cout<<"Veuillez entrer une chaine de characteres sans espaces suivit de [ENTER]\n";
                std::cin>>saisie_de_user;
                fp=open("/dev/Buf_node",O_WRONLY);
                if (fp < 0) {
                    printf("Error couldnt open the file\n");
                } else {
                    std::cout << "Fichier ouvert avec success en ecriture\n";
                    i=0;
                    while(saisie_de_user[i]!='\0'){
                        i++;
                    }
                    i++;
                    std::cout<<"Number of character is : "<<i<<'\n';
                    std::cout<<"Nombre de charactere ecrit avec succes : "<<write(fp,(char*) saisie_de_user,i)<<'\n';

                    std::cout << "Ecriture completer, fermeture du fichier\n";
                    close(fp);
                    std::cout << "Fichier fermer\n";
                }
                break;
            case '2':
                fp=open("/dev/Buf_node",O_RDONLY);
                if (fp < 0) {
                    printf("Error couldnt open the file\n");
                } else {
                    std::cout << "Fichier ouvert avec succes en lecture\n";
                    std::cout<<"Entrez le nombre de charactere a lire\n";
                    std::cin>>nb_char;
                    std::cout<<"Nombre de charactere lu : "<<read(fp,(char *) lecture_fichier,nb_char)<<"\n";
                    for(i=0;i<nb_char;i++){
                        std::cout<<lecture_fichier[i];
                    }
                    std::cout <<"\n";
                    close(fp);
                    std::cout << "Fichier fermer\n";
                    for(int i=0;i<256;i++){
                        lecture_fichier[i]='\0';
                    }
                }
                break;
            case '3':
                fp=open("/dev/Buf_node",O_RDONLY);
                if (fp < 0) {
                    printf("Error couldnt open the file\n");
                } else {
                    close(fp);
                }
                break;
            case '4':

                fp=open("/dev/Buf_node",O_RDWR);
                if (fp < 0 ) {
                    printf("Error couldnt open the file\n");
                } else {
                    std::cout<<"Nombre de donnee    : "<<ioctl(fp,BUFF_GETNUMDATA)<<"\n";
                    std::cout<<"Nombre de lecteur   : "<<ioctl(fp,BUFF_GETNUMREADER)<<"\n";
                    std::cout<<"Taille du Buffer    : "<<ioctl(fp,BUFF_GETBUFSIZE)<<"\n";
                    std::cout<<"Voulez-vous modifier la taille du buffer (Y/n)\n";
                    std::cin>>input;
                    if(input=='Y' || input =='y'){
                        std::cout<<"Veuillez entrez la taille du nouveau buffer\n";
                        std::cin>>taille;
                        if(taille>0 && taille<32000){
                            ioctl(fp,BUFF_SETBUFSIZE,taille);
                            std::cout<<"Changement effectuer\n";
                        }
                        else{
                            std::cout<<"taille innaproprie\n";
                        }

                    }
                    close(fp);
                }

                break;
            default:
                break;
        }

        if(input=='q'){
            break;
        }
        std::this_thread::sleep_for (std::chrono::seconds(3));
    }

    //fprintf(fp, "%s",buffer);



   return 0;
}