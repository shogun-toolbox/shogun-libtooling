#include <memory>

class Labels{};

class Machine{
public:
    std::shared_ptr<Labels> m_labels;
};

class test: public Machine{

public:
    void run(){
        auto t = std::make_shared<Labels>();
        m_labels = t; // can match 

        m_labels = t; // can't match?
    }
    //can't match run3?
     void run3(){
        auto t = std::make_shared<Labels>();
        m_labels = t;

        m_labels = t;
    }

    void run2(){
        auto t = m_labels; // can match

        auto t2 = m_labels; // can't match?
    }
};