#ifndef DtlsFactory_hxx
#define DtlsFactory_hxx

#include <memory>

#include "DtlsTimer.hxx"

typedef struct x509_st X509;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct evp_pkey_st EVP_PKEY;

namespace dtls
{

class DtlsSocket;
class DtlsSocketContext;
class DtlsTimerContext;

//Not threadsafe. Timers must fire in the same thread as dtls processing.
class DtlsFactory
{
   public:
     enum PacketType { rtp, dtls, stun, unknown};
     
     DtlsFactory(std::auto_ptr<DtlsTimerContext> tc, X509 *cert, EVP_PKEY *privkey);

     // Note: this orphans any DtlsSockets you were stupid enough
     // not to free
     ~DtlsFactory();

     
     DtlsSocket* createClient(std::auto_ptr<DtlsSocketContext> context);
     DtlsSocket* createServer(std::auto_ptr<DtlsSocketContext> context);
     void getMyCertFingerprint(char *fingerprint);
     DtlsTimerContext& getTimerContext() {return *mTimerContext;}
     void setSrtpProfiles(const char *policyStr);
     void setCipherSuites(const char *cipherSuites);
     static const char* DefaultSrtpProfile; 

     static PacketType demuxPacket(const unsigned char *buf, unsigned int len);
     

      //context accessor
private:
     friend class DtlsSocket;
     SSL_CTX* mContext;
     std::auto_ptr<DtlsTimerContext> mTimerContext;
     X509 *mCert;
};

}
#endif
