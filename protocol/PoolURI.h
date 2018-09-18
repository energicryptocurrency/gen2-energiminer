#pragma once

#include <string>

// A simple URI parser specifically for mining pool endpoints
enum class SecureLevel {NONE = 0, TLS12, TLS};
enum class ProtocolFamily {GETWORK = 0, STRATUM};

class URI
{
public:
    //URI() delete;
    URI(const std::string uri);

    std::string Scheme() const { return m_scheme; }
    std::string Host() const { return m_host; }
    std::string Path() const { return m_path; }
    unsigned short Port() const { return m_port; }
    std::string User() const { return m_username; }
    std::string Pass() const { return m_password; }
    SecureLevel SecLevel() const;
    ProtocolFamily Family() const;
    unsigned Version() const;
    std::string String() const { return m_uri; }
    bool Valid() const { return m_valid; }

    bool KnownScheme();

    static std::string KnownSchemes(ProtocolFamily family);

    void SetStratumMode(unsigned mode, bool confirmed)
    {
        m_stratumMode = mode;
        m_stratumModeConfirmed = confirmed;
    }
    void SetStratumMode(unsigned mode) { m_stratumMode = mode; }
    unsigned StratumMode() { return m_stratumMode; }
    bool StratumModeConfirmed() { return m_stratumModeConfirmed; }
    bool IsUnrecoverable() { return m_unrecoverable; }
    void MarkUnrecoverable() { m_unrecoverable = true; }

private:
    std::string m_scheme;
    std::string m_host;
    std::string m_path;
    std::string m_query;
    std::string m_fragment;
    std::string m_username;
    std::string m_password;
    std::string m_uri;
    unsigned short m_stratumMode = 999;  // Initial value 999 means not tested yet
    unsigned short m_port = 0;
    bool m_valid = false;
    bool m_stratumModeConfirmed = false;
    bool m_unrecoverable = false;
};
