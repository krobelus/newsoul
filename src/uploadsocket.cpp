/*  newsoul - A SoulSeek client written in C++
    Copyright (C) 2006-2007 Ingmar K. Steen (iksteen@gmail.com)
    Copyright 2008 little blue poney <lbponey@users.sourceforge.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 */

#include "uploadsocket.h"
#include "ticketsocket.h"

newsoul::UploadSocket::UploadSocket(newsoul::Newsoul * newsoul, newsoul::Upload * upload)
              : UserSocket(newsoul, "F")
{
    m_Upload = upload;

    mHavePos = false;

    m_lastDataSentCount = 0;

    m_Upload->setState(TS_Establishing);

    // Connect disconnected event.
    disconnectedEvent.connect(this, &UploadSocket::onDisconnected);
    cannotConnectEvent.connect(this, &UploadSocket::onCannotConnect);
    dataSentEvent.connect(this, &UploadSocket::onDataSent);
    dataReceivedEvent.connect(this, &UploadSocket::onDataReceived);
}

newsoul::UploadSocket::~UploadSocket()
{
    NNLOG("newsoul.up.debug", "UploadSocket destroyed");
}

/*
    Called when the connection is lost
*/
void
newsoul::UploadSocket::onDisconnected(ClientSocket * socket)
{
	if(m_Upload->state() == TS_RemoteError || m_Upload->state() == TS_LocalError)
		return;

	NNLOG("newsoul.up.debug", "UploadSocket disconnected");

	if(m_Upload->position() >= m_Upload->size())
		m_Upload->setState(TS_Finished);
	else
		m_Upload->setState(TS_ConnectionClosed);
}

/*
    Called when the connection cannot be established
*/
void
newsoul::UploadSocket::onCannotConnect(ClientSocket * socket)
{
	if(m_Upload->state() == TS_RemoteError || m_Upload->state() == TS_LocalError)
		return;

	NNLOG("newsoul.up.debug", "UploadSocket connection cannot be established");
	m_Upload->setState(TS_CannotConnect);
	disconnect();
}

void
newsoul::UploadSocket::send(const unsigned char * data, size_t n)
{
    ClientSocket::send(data, n);
    m_lastDataSentCount = sendBuffer().count();
}

void
newsoul::UploadSocket::wait()
{
    if (m_DataTimeout.isValid())
        newsoul()->reactor()->removeTimeout(m_DataTimeout);

    m_DataTimeout = newsoul()->reactor()->addTimeout(120000, this, &UploadSocket::dataTimeout);

    // Wait for an incoming connection (via TicketSocket).
    m_Upload->setState(TS_Waiting);
    newsoul()->uploads()->transferTicketReceivedEvent.connect(this, &UploadSocket::onTransferTicketReceived);
}

/*
    Stops this socket
*/
void
newsoul::UploadSocket::stop()
{
    NNLOG("newsoul.up.debug", "Disconnecting upload socket...");
    disconnect();
}

/*
    When we're trying to initiate an upload, after sending a PTransferRequest, we get a PTransferReply
    and then we need to send the ticket (right here). We'll receive the position and then we'll begin to send the data
*/
void newsoul::UploadSocket::sendTicket() {
    // Send the ticket
    char buf[4];
    uint64 ticket = m_Upload->ticket();
    for(int i = 0; i < 4; ++i)
    {
      buf[i] = (ticket >> (i * 8)) & 0xff;
    }


    if (m_DataTimeout.isValid())
        newsoul()->reactor()->removeTimeout(m_DataTimeout);

    m_DataTimeout = newsoul()->reactor()->addTimeout(60000, this, &UploadSocket::dataTimeout);

    send((const unsigned char *) &buf, 4);
    m_Upload->setState(TS_Waiting);
    NNLOG("newsoul.ticket.debug", "Ticket %u has been sent", ticket);
}

/*
    Called when some data (probably the position) has been received
*/
void newsoul::UploadSocket::onDataReceived(NewNet::ClientSocket * socket) {
    if(m_Upload->state() == TS_Waiting) {
        NNLOG("newsoul.up.debug", "got %u bytes in uploadsocket", receiveBuffer().count());

        findPosition();
    }
}

/*
    Called when the data has been sent and we need to send new data
*/
void newsoul::UploadSocket::onDataSent(NewNet::ClientSocket * socket) {
    if (m_Upload->state() == TS_Transferring) {
        if (m_DataTimeout.isValid())
            newsoul()->reactor()->removeTimeout(m_DataTimeout);

        m_DataTimeout = newsoul()->reactor()->addTimeout(60000, this, &UploadSocket::dataTimeout);

        size_t sent = 0;
        if (m_lastDataSentCount > sendBuffer().count())
            sent = m_lastDataSentCount - sendBuffer().count();

        m_Upload->sent(sent);
        m_lastDataSentCount = sendBuffer().count();

        if(sendBuffer().count() < 10240 && (m_Upload->position() + (uint64) sendBuffer().count() < m_Upload->size())) {
            if(! m_Upload->read(sendBuffer())) {
                NNLOG("newsoul.up.debug", "read error");
                m_Upload->setLocalError("File error");
                stop();
            }
        }
    }
}

/*
    The upload has been initiated by the downloader (sending a PTransferRequest). We have replied with a PTransferReply.
    And now, the downloader sends us the ticket and the position where to start the upload.
*/
void
newsoul::UploadSocket::onTransferTicketReceived(TicketSocket * socket)
{
    if((m_Upload->state() == TS_Waiting) && (m_Upload->ticket() == socket->ticket()) && (m_Upload->user() == socket->user())) {
        // Steal the socket and its data.
        setDescriptor(socket->descriptor());
        setSocketState(SocketConnected);
        receiveBuffer() = socket->receiveBuffer();
        sendBuffer() = socket->sendBuffer();

        NNLOG("newsoul.up.debug", "got %u bytes in uploadsocket", receiveBuffer().count());

        findPosition();
    }
}

/*
    Tries to get the position given by the downloader in the data taken in the receive buffer
*/
void
newsoul::UploadSocket::findPosition() {
    if(!mHavePos && receiveBuffer().count() >= 8) {
        // Getting the position
        uint64 pos = 0;
        for(int i = 0; i < 8; i++) {
            pos += receiveBuffer().data()[0] << (i*8);
            receiveBuffer().seek(1);
        }
        NNLOG("newsoul.up.debug", "Uploading from pos %i", pos);

        // Try to seek
        if(! m_Upload->seek(pos)) {
            NNLOG("newsoul.up.warn", "seek error");
            m_Upload->setLocalError("File error");
            stop();
            return;
        }

        // It seems this pos is correct
        mHavePos = true;

        // Try to send the data
        if(! m_Upload->read(sendBuffer())) {
            NNLOG("newsoul.up.warn", "read error");
            m_Upload->setLocalError("File error");
            stop();
            return;
        }
        NNLOG("newsoul.up.debug", "have %i in sending buffer", sendBuffer().count());

        // Change the state.
        m_Upload->setState(TS_Transferring);
    }

    if(mHavePos)
        // We're not supposed to receive something else... throw it away
        receiveBuffer().clear();
}

/*
    Called when we cannot send any data in this socket
*/
void
newsoul::UploadSocket::dataTimeout(long) {
    NNLOG("newsoul.up.debug", "Data timeout while uploading.");
    stop();
}
