//server6b.cpp - rodar no raspberry
//compila server6b -ocv
//testa sendBytes e receiveBytes
#include "../projeto.hpp"
int main(void) {
SERVER server;
server.waitConnection();
const int n=100000;
BYTE buf[n];
memset(buf,111,n);
server.sendBytes(n,(BYTE*)buf);
printf("Enviei 100000 bytes 111\n");
server.receiveBytes(n,(BYTE*)buf);
if (testaBytes(buf,214,n)) printf("Recebi corretamente %d bytes %d\n",n,214);
else printf("Erro na recepcao de %d bytes %d\n",n,214);
memset(buf,111,n);
server.sendBytes(n,(BYTE*)buf);
printf("Enviei 100000 bytes 111\n");
}