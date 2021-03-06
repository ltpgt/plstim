// src/client/client.cc – Client controller
//
// Copyright © 2012–2015 University of California, Irvine
// Licensed under the Simplified BSD License.

#pragma once

#include <QtNetwork>


namespace plstim {
class Client : public QObject
{
    Q_OBJECT
public:
    Client ()
    {
	connect (&m_socket, SIGNAL (encrypted ()),
		 this, SLOT (ready ()));
	connect (&m_socket, SIGNAL (sslErrors (const QList<QSslError>&)),
		 this, SLOT (onSslErrors (const QList<QSslError>&)));
	m_socket.connectToHostEncrypted ("localhost", 3928);
	m_socket.waitForEncrypted (4000);
	m_socket.write ("Hello world\n");
    }
public slots:
    void onSslErrors (const QList<QSslError>& errors)
    {
	qDebug () << "SSL errors:";
    }

    void ready ()
    {
	qDebug () << "Encrypted connection ready";
    }
protected:
    QSslSocket m_socket;
};
} // namespace plstim

