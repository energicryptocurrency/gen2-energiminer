#pragma once

#include <random>
#include <string>

class ExtraNonceSingleton
{
private:
    static ExtraNonceSingleton* s_instance;

private:
    uint32_t m_extraNonce;

    //! {@brief random components
private:
    std::random_device                      m_device;
    std::mt19937                            m_engine;
    std::uniform_int_distribution<uint32_t> m_distribution;
    //!}

private:
    /**
     * @brief Default constructor
     **/
    ExtraNonceSingleton();

    /**
     * @brief Copy constructor
     **/
    ExtraNonceSingleton (const ExtraNonceSingleton&) = delete;

    /**
     * @brief Copy assignment operator
     **/
    ExtraNonceSingleton& operator=(const ExtraNonceSingleton&) = delete;

    /**
     * @brief Destructor
     **/
    ~ExtraNonceSingleton();

public:
    uint32_t getExtraNonce() const;

    std::string toString() const;

    void generateExtraNonce();

    std::string genAndSendExtraNonce();

public:
    /**
     * @brief create the singleton object if it is not exist and returns this object
     *  \ if it is exist returns it.
     **/
    static ExtraNonceSingleton& getInstance();

    /**
     * @brief removes the singleton object
     **/
    static void removeInstance();
};

