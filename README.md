lab1

Ce laboratoire contient un CHAR DRIVER fonctionnel avec le kernel de linux. 
En exécutant le script final.sh il vous sera possible d'installer et de desintaller le driver
en plus de pouvoir lancer le banc de test. Ce dernier permet l'écriture et la lecture
du driver en mode bloquant ou non bloquant. De plus il est également possible de changer
la taille du buffer grâce à la fonction IOCTL.

Puisque le driver se situe dans le répertoire /dev/ l'utilisateur devra etre en mode super user
