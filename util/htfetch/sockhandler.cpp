#include "sockhandler.h"

#include <util/datetime/base.h>

bool                                        TSslSocketBase::Initialized = false;
sslKeys_t *                                 TSslSocketBase::Keys = NULL;
THolder<TSslSocketBase::TBufferAllocator>   TSslSocketBase::BufAlloc;

bool TSslSocketBase::StaticInit(const char *caFile) {
    VERIFY(!Initialized, "SslSocket StaticInit: already initialized");
    BufAlloc.Reset(new TSslSocketBase::TBufferAllocator);
    if (matrixSslOpen() < 0)
        VERIFY(0, "SslSocket StaticInit: unable to initialize matrixSsl");
    VERIFY(caFile && caFile[0], "SslSocket StaticInit: no certificate authority file");

    if (matrixSslReadKeys(&Keys, NULL, NULL, NULL, caFile) < 0) {
        VERIFY(0, "SslSocket StaticInit: unable to load ssl keys from %s", caFile);
    }
    Initialized = true;
    return Initialized;
}

void TSslSocketBase::StaticTerm() {
    VERIFY(Initialized, "SslSocket StaticTerm: not initialized");
    matrixSslFreeKeys(Keys);
    matrixSslClose();
    Keys = NULL;
    BufAlloc.Reset(NULL);
    Initialized = false;
}

bool MatchPattern(const char *p, const char *pe, const char *s, const char *se, int maxAsteriskNum) {
    if (maxAsteriskNum <= 0)
        return false;
    while (p < pe && s < se) {
        if (*p == '*') {
            ++p;
            while (p < pe && *p == '*')
                ++p;
            while (s < se) {
                if (MatchPattern(p, pe, s, se, maxAsteriskNum - 1))
                    return true;
                if (*s == '.')
                    return false;
                ++s;
            }
            return p == pe;
        } else {
            if (*p != *s)
                return false;
            ++p;
            ++s;
        }
    }
    while (p < pe && *p == '*')
        ++p;
    return (p == pe && s == se);
}

bool MatchHostName(const char *pattern, size_t patternLen, const char *name, size_t nameLen) {
    // rfc 2818 says:
    // wildcard character * can match any single domain name component or component fragment
    VERIFY(name && nameLen, "Ssl certificate check error: hostname is empty");
    if (!pattern || !patternLen)
        return false;
    const char *ne = strchr(name, ':');
    if (!ne || ne > name + nameLen)
        ne = name + nameLen;
    return MatchPattern(pattern, pattern + patternLen, name, ne, 5);
}

bool IsExpired(const char *notBefore, const char *notAfter) {
    time_t notbefore, notafter;
    if (!ParseX509ValidityDateTime(notBefore, notbefore) ||
        !ParseX509ValidityDateTime(notAfter, notafter))
        return true;
    time_t t = Seconds();
    return notbefore > t || t > notafter;
}

int TSslSocketBase::CertChecker(sslCertInfo_t *cert, void *arg)
{
    YASSERT(cert);
    YASSERT(arg);
    TSocketCtx *ctx = (TSocketCtx *)arg;
    ctx->CertErrors = 0;

    // matching hostname
    if (ctx->Host && ctx->HostLen) {
        bool nameMatched = false;
        sslSubjectAltNameEntry *an = cert->subjectAltName;
        while (an) {
            // dNSName id is 2.
            if (an->id == 2 && MatchHostName((const char *)an->data, an->dataLen, ctx->Host, ctx->HostLen)) {
                nameMatched = true;
                break;
            }
            an = an->next;
        }
        if (!nameMatched && cert->subject.commonName) {
            nameMatched = MatchHostName(cert->subject.commonName, strlen(cert->subject.commonName), ctx->Host, ctx->HostLen);
        }
        if (!nameMatched)
            ctx->CertErrors |= SSL_CERT_HOSTNAME_MISMATCH;
    }

    // walk through certificate chain and check if they are signed correctly and not expired
    sslCertInfo_t *c = cert;
    while (c->next) {
        if (IsExpired(c->notBefore, c->notAfter))
            ctx->CertErrors |= SSL_CERT_EXPIRED;
        if (c->verified < 0) {
            ctx->CertErrors |= SSL_CERT_BAD_CHAIN;
        }
        c = c->next;
    }
    if (c->verified < 0)
        ctx->CertErrors |= SSL_CERT_UNTRUSTED;
    if (IsExpired(c->notBefore, c->notAfter))
        ctx->CertErrors |= SSL_CERT_EXPIRED;

    return SSL_ALLOW_ANON_CONNECTION;
}
