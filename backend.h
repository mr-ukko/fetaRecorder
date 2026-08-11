#ifndef BACKEND_H
#define BACKEND_H

#include <QObject>
#include <QDebug>


class BackEnd : public QObject
{
    Q_OBJECT

public:
    int d;
    explicit BackEnd(QObject *parent = nullptr) : QObject(parent) {
        d=0;
    }
    Q_INVOKABLE void handleButtonClick();
    Q_INVOKABLE void testSS();
    
};


#endif
