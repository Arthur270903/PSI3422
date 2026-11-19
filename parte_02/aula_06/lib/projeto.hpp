//projeto.hpp
#include "raspberry.hpp"
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <vector>

bool testaBytes(BYTE* buf, BYTE b, int n) {
  for (int i=0; i<n; i++) if (buf[i]!=b) return false;
  return true;
}

bool testaVb(const vector<BYTE> vb, BYTE b) {
  for (unsigned i=0; i<vb.size(); i++) if (vb[i]!=b) return false;
  return true;
}

class DEVICE {
public:
  virtual ~DEVICE() {}
  static void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) return &(((struct sockaddr_in*)sa)->sin_addr);
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
  }
  virtual void sendBytes(int nBytesToSend, BYTE *buf)=0;
  virtual void receiveBytes(int nBytesToReceive, BYTE *buf)=0;

  // ---- Licao 2a: inteiros (usa htonl/ntohl) ----
  void sendUint(uint32_t m) {
    static uint32_t t;
    t = htonl(m);                 // host -> rede (big endian)
    sendBytes(4, (BYTE*)&t);
  }
  void receiveUint(uint32_t& m) {
    static uint32_t t;
    receiveBytes(4, (BYTE*)&t);
    m = ntohl(t);                 // rede -> host
  }

  // ---- Licao 2b: vector<BYTE> (cabecalho + dados) ----
  void sendVb(const vector<BYTE>& vb) {
    sendUint(vb.size());
    if (vb.size()>0) sendBytes(vb.size(), (BYTE*)vb.data());
  }
  void receiveVb(vector<BYTE>& vb) {
    uint32_t t; receiveUint(t);
    vb.resize(t);
    if (t>0) receiveBytes(t, vb.data());
  }

  // ---- Licao 3a: imagem sem compressao ----
  void sendImg(const Mat_<COR>& img) {
    if (!img.isContinuous()) erro("sendImg: imagem nao continua");
    sendUint(img.rows); sendUint(img.cols);
    sendBytes(3*img.total(), img.data);
  }
  void receiveImg(Mat_<COR>& img) {
    uint32_t nl, nc;
    receiveUint(nl); receiveUint(nc);
    img.create(nl, nc);
    receiveBytes(3*img.total(), img.data);
  }

  // ---- Licao 3b: imagem JPEG ----
  void sendImgComp(const Mat_<COR>& img) {
    vector<BYTE> vb;
    vector<int> param{IMWRITE_JPEG_QUALITY, 80};
    imencode(".jpg", img, vb, param);
    sendVb(vb);
  }
  void receiveImgComp(Mat_<COR>& img) {
    vector<BYTE> vb;
    receiveVb(vb);
    img = imdecode(vb, 1);        // 1 = colorida
  }
};

class SERVER: public DEVICE {
  const string PORT="3490";
  const int BACKLOG=1;
  int sockfd=-1, new_fd=-1;
public:
  SERVER() {
    struct addrinfo hints, *servinfo, *p;
    int yes=1, rv;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT.c_str(), &hints, &servinfo)) != 0)
      erro(string("getaddrinfo: ")+gai_strerror(rv));
    for (p=servinfo; p!=NULL; p=p->ai_next) {
      if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
        perror("server: socket"); continue; }
      if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        erro("setsockopt");
      if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
        close(sockfd); perror("server: bind"); continue; }
      break;
    }
    freeaddrinfo(servinfo);
    if (p == NULL) erro("server: failed to bind");
    if (listen(sockfd, BACKLOG) == -1) erro("listen");
  }
  ~SERVER() {
    if (new_fd!=-1) close(new_fd);
    if (sockfd!=-1) close(sockfd);
    printf("server: fechei conexao.\n");
  }
  void waitConnection() {
    struct sockaddr_storage their_addr;
    socklen_t sin_size = sizeof their_addr;
    char s[INET6_ADDRSTRLEN];
    printf("server: Esperando conexao...\n");
    while (1) {
      new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
      if (new_fd == -1) { perror("accept"); continue; }
      break;
    }
    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
    printf("server: recebi conexao de %s\n", s);
    close(sockfd); sockfd=-1;     // nao precisa mais do listener
  }
  void sendBytes(int nBytesToSend, BYTE *buf) {
    int total=0;
    while (total < nBytesToSend) {
      int n = send(new_fd, buf+total, nBytesToSend-total, MSG_NOSIGNAL);
      if (n <= 0) erro("server: erro em send");
      total += n;
    }
  }
  void receiveBytes(int nBytesToReceive, BYTE *buf) {
    int total=0;
    while (total < nBytesToReceive) {
      int n = recv(new_fd, buf+total, nBytesToReceive-total, 0);
      if (n <= 0) erro("server: erro em recv ou conexao fechada");
      total += n;
    }
  }
};

class CLIENT: public DEVICE {
  const string PORT="3490";
  int sockfd=-1;
public:
  CLIENT(string endereco) {
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if ((rv = getaddrinfo(endereco.c_str(), PORT.c_str(), &hints, &servinfo)) != 0)
      erro(string("getaddrinfo: ")+gai_strerror(rv));
    for (p=servinfo; p!=NULL; p=p->ai_next) {
      if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
        perror("client: socket"); continue; }
      if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
        perror("client: connect"); close(sockfd); continue; }
      break;
    }
    if (p == NULL) erro("client: failed to connect");
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    printf("client: conectando a %s\n", s);
    freeaddrinfo(servinfo);
  }
  ~CLIENT() { if (sockfd!=-1) close(sockfd); }
  void sendBytes(int nBytesToSend, BYTE *buf) {
    int total=0;
    while (total < nBytesToSend) {
      int n = send(sockfd, buf+total, nBytesToSend-total, MSG_NOSIGNAL);
      if (n <= 0) erro("client: erro em send");
      total += n;
    }
  }
  void receiveBytes(int nBytesToReceive, BYTE *buf) {
    int total=0;
    while (total < nBytesToReceive) {
      int n = recv(sockfd, buf+total, nBytesToReceive-total, 0);
      if (n <= 0) erro("client: erro em recv ou conexao fechada");
      total += n;
    }
  }
};
