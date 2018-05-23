#include <iomanip>
#include <sstream>

#include "extranoncesingleton.h"

ExtraNonceSingleton* ExtraNonceSingleton::s_instance = nullptr;

ExtraNonceSingleton::ExtraNonceSingleton()
    : m_extraNonce{0}
    , m_device{}
    , m_engine{m_device()}
{
}

void ExtraNonceSingleton::generateExtraNonce()
{
    m_extraNonce = m_distribution(m_engine);
}

uint32_t ExtraNonceSingleton::getExtraNonce() const
{
    return m_extraNonce;
}

std::string ExtraNonceSingleton::toString() const
{
    std::stringstream stream;
    stream << std::setfill ('0') << std::setw(sizeof(m_extraNonce) * 2)
           << std::hex << m_extraNonce;
    return stream.str();
}

std::string ExtraNonceSingleton::genAndSendExtraNonce()
{
    generateExtraNonce();
    return toString();
}

ExtraNonceSingleton* ExtraNonceSingleton::getInstance()
{
    if (nullptr == s_instance) {
        s_instance = new ExtraNonceSingleton();
    }
    return s_instance;
}

void ExtraNonceSingleton::removeInstance()
{
    delete s_instance;
    s_instance = nullptr;
}

ExtraNonceSingleton::~ExtraNonceSingleton()
{
}
