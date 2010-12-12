/*
 SerPro - A serial protocol for arduino intercommunication
 Copyright (C) 2009-2010 Alvaro Lopes <alvieboy@alvie.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 3 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General
 Public License along with this library; if not, write to the
 Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301 USA
 */

#include <SerPro-glib.h>
#include <fcntl.h>

void SerialWrapper::write(uint8_t v) {
	GError *error = NULL;
	gsize written;
	g_io_channel_write_chars(SerProGLIB::channel,(const gchar*)&v,sizeof(v),&written,&error);
}

void SerialWrapper::write(const unsigned char *buf, unsigned int size) {
	GError *error = NULL;
	gsize written;
	g_io_channel_write_chars(SerProGLIB::channel,(const gchar*)buf,size,&written,&error);
}

void SerialWrapper::flush() {
	GError *error = NULL;
	g_io_channel_flush(SerProGLIB::channel,&error);
}

bool SerialWrapper::waitEvents(bool block) {
	return g_main_context_iteration(g_main_context_default(), TRUE);
}

GLIBTimer::timer_t GLIBTimer::addTimer( int (*cb)(void*), int milisseconds, void *data)
{
	return g_timeout_add(milisseconds,cb,data);
}

GLIBTimer::timer_t GLIBTimer::cancelTimer(const timer_t &t) {
	g_source_remove(t);
	return 0;
}

int SerProGLIB::init(const char *device, speed_t baudrate)
{
	fd = open(device, O_RDWR|O_NOCTTY|O_NONBLOCK);
	if (fd<0) {
		perror("open");
		return -1;
	}
	return init(fd,baudrate);
}

int SerProGLIB::init(int fdpty, speed_t baudrate)
{
	struct termios termset;
	GError *error = NULL;
	int status;

	loop = g_main_loop_new( g_main_context_default(), TRUE );

	fd=fdpty;

	tcgetattr(fd, &termset);
	termset.c_iflag = IGNBRK;

	termset.c_oflag &= ~OPOST;
	termset.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	termset.c_cflag &= ~(CSIZE | PARENB| HUPCL);
	termset.c_cflag |= CS8;
	termset.c_cc[VMIN]=1;
	termset.c_cc[VTIME]=5;

	cfsetospeed(&termset,baudrate);
	cfsetispeed(&termset,baudrate);

	tcsetattr(fd,TCSANOW,&termset);

	ioctl(fd, TIOCMGET, &status); 

	status |= ( TIOCM_DTR | TIOCM_RTS );

	ioctl(fd, TIOCMSET, &status);


	channel =  g_io_channel_unix_new(fd);

	if (NULL==channel) {
		fprintf(stderr,"Cannot open channel\n");
		return -1;
	}
	g_io_channel_set_encoding (channel, NULL, &error);
	if (error) {
		fprintf(stderr,"Cannot set encoding: %s\n", error->message);
	}
	error = NULL;

	g_io_channel_set_buffered(channel, false);
	

	g_io_channel_set_flags (channel, G_IO_FLAG_NONBLOCK, &error);
	if (error) {
		fprintf(stderr,"Cannot set flags: %s\n", error->message);
	}
	g_io_channel_set_close_on_unref (channel, TRUE);

	if ((watcher=g_io_add_watch(channel, G_IO_IN, &serial_data_ready, NULL))<0) {
		fprintf(stderr,"Cannot add watch\n");
	}

	fprintf(stderr,"Channel set up OK\n");

	in_request = FALSE;
	delay_request = FALSE;

	return 0;
}

void SerProGLIB::run()
{
	g_main_loop_run(loop);
}

void SerProGLIB::runEvents()
{
	g_main_context_iteration(g_main_context_default(), TRUE);
}

void SerProGLIB::waitConnection()
{
	do {
		SerProGLIB::runEvents();
	} while (!connected);

}

gboolean SerProGLIB::serial_data_ready(GIOChannel *source,GIOCondition condition,gpointer data)
{
	gsize r,i;
	gchar c[128];
	GError *error = NULL;

	g_io_channel_read_chars(source,c,sizeof(c),&r,&error);
	if (NULL==error)  {
		for (i=0;i<r;i++)
			process((unsigned char)c[i]);
	}
	return TRUE;
}

void SerProGLIB::onConnect(void (*func)(void))
{
	connectCB = func;

}
gboolean connectEventImpl(gpointer arg)
{
	void (*func)(void) = (void(*)(void))arg;
	if (func)
		func();
	return FALSE;
}

void SerProGLIB::connectEvent() {
	connected = true;
    g_timeout_add( 0, &connectEventImpl, (void*)connectCB);
}


GIOChannel *SerProGLIB::channel;
gint SerProGLIB::watcher;
int SerProGLIB::fd;
gboolean SerProGLIB::in_request, SerProGLIB::delay_request;
void (*SerProGLIB::connectCB)() = NULL;
GMainLoop *SerProGLIB::loop;
bool SerProGLIB::connected=false;