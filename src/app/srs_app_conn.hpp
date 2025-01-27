#ifndef SRS_APP_CONN_HPP
#define SRS_APP_CONN_HPP

#include <map>
#include <vector>
#include <openssl/ssl.h>
#include <srs_core.hpp>
#include <srs_protocol_st.hpp>
#include <srs_protocol_io.hpp>
#include <srs_protocol_conn.hpp>
#include <srs_app_st.hpp>

// Hooks for connection manager, to handle the event when disposing connections.
class ISrsDisposingHandler
{
public:
    ISrsDisposingHandler();
    virtual ~ISrsDisposingHandler();
public:
    // When before disposing resource, trigger when manager.remove(c), sync API.
    // @remark Recommend to unref c, after this, no other objects refs to c.
    virtual void on_before_dispose(ISrsResource* c) = 0;
    // When disposing resource, async API, c is freed after it.
    // @remark Recommend to stop any thread/timer of c, after this, fields of c is able
    // to be deleted in any order.
    virtual void on_disposing(ISrsResource* c) = 0;
};

// The resource manager remove resource and delete it asynchronously.
class SrsResourceManager : public ISrsCoroutineHandler, public ISrsResourceManager
{
private:
    std::string label_;
    SrsContextId cid_;
    bool verbose_;
private:
    SrsCoroutine* trd;
    srs_cond_t cond;
   // Callback handlers.
    std::vector<ISrsDisposingHandler*> handlers_;
    // Unsubscribing handlers, skip it for notifying.
    std::vector<ISrsDisposingHandler*> unsubs_;
    // Whether we are removing resources.
    bool removing_;
    // // The zombie connections, we will delete it asynchronously.
    std::vector<ISrsResource*> zombies_;
    std::vector<ISrsResource*>* p_disposing_;
private:
    // The connections without any id.
    std::vector<ISrsResource*> conns_;
    // The connections with resource id.
    std::map<std::string, ISrsResource*> conns_id_;
    // The connections with resource fast(int) id.
    std::map<uint64_t, ISrsResource*> conns_fast_id_;
    // The level-0 fast cache for fast id.
    int nn_level0_cache_;
    // SrsResourceFastIdItem* conns_level0_cache_;
    // The connections with resource name.
    std::map<std::string, ISrsResource*> conns_name_;
public:
    SrsResourceManager(const std::string& label, bool verbose = false);
    virtual ~SrsResourceManager(); 
public:
    srs_error_t start();
// Interface ISrsCoroutineHandler
public:
    virtual srs_error_t cycle();    
public:
    void add(ISrsResource* conn, bool* exists = NULL);
// Interface ISrsResourceManager
public:
    virtual void remove(ISrsResource* c);
private:
    void do_remove(ISrsResource* c);
    void check_remove(ISrsResource* c, bool& in_zombie, bool& in_disposing);
    void clear();
    void do_clear();
    void dispose(ISrsResource* c);
};

// If a connection is able to be expired,
// user can use HTTP-API to kick-off it.
class ISrsExpire
{
public:
    ISrsExpire();
    virtual ~ISrsExpire();
public:
    // Set connection to expired to kick-off it.
    virtual void expire() = 0;
};


// The basic connection of SRS, for TCP based protocols,
// all connections accept from listener must extends from this base class,
// server will add the connection to manager, and delete it when remove.
class SrsTcpConnection : public ISrsProtocolReadWriter
{
private:
    // The underlayer st fd handler.
    srs_netfd_t stfd;
    // The underlayer socket.
    SrsStSocket* skt;
public:
    SrsTcpConnection(srs_netfd_t c);
    virtual ~SrsTcpConnection();    
// Interface ISrsProtocolReadWriter
public:
    // Set socket option TCP_NODELAY.
    virtual srs_error_t set_tcp_nodelay(bool v);
    // // Set socket option SO_SNDBUF in srs_utime_t.
    // virtual srs_error_t set_socket_buffer(srs_utime_t buffer_v);
    virtual int get_fd();
public:
    virtual void set_recv_timeout(srs_utime_t tm);
    virtual srs_utime_t get_recv_timeout();
    virtual srs_error_t read_fully(void* buf, size_t size, ssize_t* nread);
    virtual int64_t get_recv_bytes();
    virtual int64_t get_send_bytes();
    virtual srs_error_t read(void* buf, size_t size, ssize_t* nread);
    virtual void set_send_timeout(srs_utime_t tm);
    virtual srs_utime_t get_send_timeout();
    virtual srs_error_t write(void* buf, size_t size, ssize_t* nwrite);
    virtual srs_error_t writev(const iovec *iov, int iov_size, ssize_t* nwrite);
};

// The SSL connection over TCP transport, in server mode.
class SrsSslConnection : public ISrsProtocolReadWriter
{
private:
    // The under-layer plaintext transport.
    ISrsProtocolReadWriter* transport;
private:
    SSL_CTX* ssl_ctx;
    SSL* ssl;
    BIO* bio_in;
    BIO* bio_out;
public:
    SrsSslConnection(ISrsProtocolReadWriter* c);
    virtual ~SrsSslConnection();
public:
    virtual srs_error_t handshake(std::string key_file, std::string crt_file);
    virtual srs_error_t handshake(X509* cert, EVP_PKEY* key);
// Interface ISrsProtocolReadWriter
public:
    virtual void set_recv_timeout(srs_utime_t tm);
    virtual srs_utime_t get_recv_timeout();
    virtual srs_error_t read_fully(void* buf, size_t size, ssize_t* nread);
    // virtual int64_t get_recv_bytes();
    // virtual int64_t get_send_bytes();
    virtual srs_error_t read(void* buf, size_t size, ssize_t* nread);
    virtual void set_send_timeout(srs_utime_t tm);
    virtual srs_utime_t get_send_timeout();
    virtual srs_error_t write(void* buf, size_t size, ssize_t* nwrite);
    virtual srs_error_t writev(const iovec *iov, int iov_size, ssize_t* nwrite);
    virtual int get_fd();
};

class SrsSslClient : public ISrsReader, public ISrsStreamWriter
{
private:
    SrsTcpClient* transport;
private:
    SSL_CTX* ssl_ctx;
    SSL* ssl;
    BIO* bio_in;
    BIO* bio_out;
    string sni_;//server_host_name
public:
    SrsSslClient(SrsTcpClient* tcp);
    virtual ~SrsSslClient();
public:
    virtual srs_error_t handshake();
public:
    virtual srs_error_t set_SNI(std::string sni);
public:
    virtual srs_error_t read(void* buf, size_t size, ssize_t* nread);
    virtual srs_error_t write(void* buf, size_t size, ssize_t* nwrite);
    virtual srs_error_t prepare_resign_endpoint(X509 *fake_x509, EVP_PKEY* server_key);
    virtual RSA* get_new_cert_rsa(int key_length);
};

#endif