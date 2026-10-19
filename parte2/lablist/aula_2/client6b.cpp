//client6b.cpp - no computador
//compila client6b -ocv -v3
//testa sendBytes e receiveBytes
#include "../projeto.hpp"
int main(int argc, char *argv[]) {
if (argc!=2) erro("client6 servidorIpAddr\n");
CLIENT client(argv[1]);
const int n=100000;
BYTE buf[n];
client.receiveBytes(n,(BYTE*)buf);
if (testaBytes(buf,111,n)) printf("Recebi corretamente %d bytes %d\n",n,111);
else printf("Erro na recepcao de %d bytes %d\n",n,111);
memset(buf,214,n);
client.sendBytes(n,(BYTE*)buf);
printf("Enviei 100000 bytes 214\n");
client.receiveBytes(n,(BYTE*)buf);
if (testaBytes(buf,2,n)) printf("Recebi corretamente %d bytes %d\n",n,2);
else printf("Nao recebi %d bytes %d\n",n,2);
}  