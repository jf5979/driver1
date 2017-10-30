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
    std::cout<<"Ecriture dans le driver\n";
    char saisie_de_user[16];
    char lecture_fichier[16];
    char input=0x00,lecture=0;
    FILE *fp;
    int file_descriptor;




    while(1){
        std::cout << "\033[2J\033[1;1H";
        std::cout<<"1. Ecrire dans le fichier\n2. Lire dans le fichier\n3. Ouvrir fermer le fichier\n4. IOCTL\n";
        std::cin>>input;
        switch (input) {
            case '1' :
                std::cout<<"Veuillez entrer une chaine de moins de 15 characteres suivit de [ENTER]";
                std::cin>>saisie_de_user;
                fp=fopen("/dev/Buf_node","w+");
                if (fp == NULL) {
                    printf("Error couldnt open the file\n");
                } else {
                    std::cout << "Fichier ouvert avec success en ecriture\n";
                    fprintf(fp, "%s",saisie_de_user);
                    std::cout << "Ecriture completer, fermeture du fichier\n";
                    fclose(fp);
                    std::cout << "Fichier fermer\n";
                }
                break;
            case '2':
                fp=fopen("/dev/Buf_node","r");
                if (fp == NULL) {
                    printf("Error couldnt open the file\n");
                } else {
                    std::cout << "Fichier ouvert avec succes en lecture\n";
                    for(int i=0;i<16;i++){
                        fscanf(fp,"%c",&(lecture_fichier[i]));
                        std::cout<<lecture_fichier[i];
                        if(lecture_fichier[i]=='\0'){
                            break;
                        }
                    }
                    std::cout << "\nLecture completer, fermeture du fichier\n";
                    fclose(fp);
                    std::cout << "Fichier fermer\n";
                    for(int i=0;i<16;i++){
                        lecture_fichier[i]='\0';
                    }
                }
                break;
            case '3':
                fp=fopen("/dev/Buf_node","r");
                if (fp == NULL) {
                    printf("Error couldnt open the file\n");
                } else {
                    fclose(fp);
                }
                break;
            case '4':

                file_descriptor=open("/dev/Buf_node",O_RDWR);
                if (file_descriptor<0 ) {
                    printf("Error couldnt open the file\n");
                } else {
                    std::cout<<"Nombre de donnee    : "<<ioctl(file_descriptor,BUFF_GETNUMDATA)<<"\n";
//                    std::cout<<"Nombre de lecteur   : "<<ioctl(file_descriptor,1)<<"\n";
//                    std::cout<<"Taille du Buffer    : "<<ioctl(file_descriptor,2)<<"\n";
                    std::cout<<"Voulez-vous modifier la taille du buffer (Y/n)";

                    close(file_descriptor);
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