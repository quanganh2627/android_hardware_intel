/*
 *  hardware/Intel/cp_daemon/cpdModem.c
 *
 * "Low level" modem operations. Open/close gsmtty handle, read/write blocks of data.
 *
 * Martin Junkar 09/18/2011
 *
 *
 */


#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <termios.h>
#include <sys/poll.h>
#include <utils/Log.h>


int modemClose(int *fd);


/*
 * Open gsmtty for modem.
 * default value for <openOptions> is 0, parameter is here for future changes.
 */
int modemOpen(char * name, int openOptions)
{
    int fd = 0;
    struct termios   options;
    int              result;

    if (name == NULL) return fd;

    if (openOptions == 0) {
        openOptions = ( O_RDWR | O_NOCTTY | O_NONBLOCK);
    }

    fd = open( name, openOptions);
    if (fd <= 0) {
        LOGE("Can't open %s, %d", name, fd);
        return fd;
    }

    /* get options: */
    result = tcgetattr(fd, &options);
    if (result != 0) {
        modemClose(&fd);
        return fd;
    }
    /* Recomended by Mbaye, ElvisX: */
    cfmakeraw(&options);

/*  Martin:
    options.c_cflag = CLOCAL | CREAD | CS8 | ~ICANON | ~O_NONBLOCK;
    options.c_iflag = IGNPAR;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_cc[VMIN] = 1;
    options.c_cc[VTIME] = 0;
*/

    /* Modem FW 1139 and later requre CLOCAL flag to be set, BZ#13041, Requested by Vincent P. , Marc B. Xiao Jin */
    /* Alek suggested these flag to be set in May 2011, later this was removed and replace by code above - Elvis. */
    /* Recomended by Du Alek:  */
    options.c_cflag |= CS8 | CLOCAL | CREAD;
    options.c_iflag &= ~(INPCK | IGNPAR | ISTRIP | IXANY | ICRNL);
    options.c_oflag &= ~OPOST;
    /* these options may not be needed, at least it seems they don't make a difference */
/*
    options.c_cc[VMIN]  = 1;
    options.c_cc[VTIME] = 10;
*/

    /* Set the new options for the port: */
    result = tcsetattr(fd, TCSANOW, &options);
    if (result != 0) {
        modemClose(&fd);
        return fd;
    }

    result = tcflush(fd, TCIOFLUSH);
    if (result != 0) {
        modemClose(&fd);
        return fd;
    }

    return fd;
}

/*
 * close modem gsmtty port and set fd to -1.
 */
int modemClose(int *fd)
{
    if (fd == NULL) return 0;
    if (*fd <= 0) return 0;
    LOGD("Closing gsmtty, %d", *fd);
    close(*fd);
    *fd = 0;
    return *fd;
}

/*
 * Write buffer into tty port referenced by fd.
 * Fail if file is closed.
 * returns number of bytes written.
 * The caller is respoonsible for error handling or  write re-try if neded.
 */
int modemWrite(int fd, void *pBuffer, int len)
{
    int result = 0;
    if (fd <= 0) return result;
    if (pBuffer == NULL) return result;
    if (len <= 0) return result;
    result = write(fd, pBuffer, len);
    if (result < 0) {
        LOGE("gsmtty write error, %d", result);
    }
    return result;
}

/*
 * Read data from modem (file).
 * Returns number of bytes read.
 * This is blocking read function. Implements pool() mechanism to block until there is data available of if there is an error on <fd>
 * Return valur 0 is a valid return reason
 * Return values < 0 are errors.
 */
int modemRead(int fd, void *pBuffer, int len)
{
    int result = -3;
    int ret;
    struct pollfd p;

    if (fd <= 0) return result;
    if (pBuffer == NULL) return result;
    if (len <= 0) return result;
    p.fd = fd;
//    p.events = POLLERR | POLLHUP | POLLNVAL | POLLIN | POLLPRI;
    p.events = POLLERR | POLLHUP | POLLIN;
    p.revents = 0;
    ret = poll( &p, 1, -1);
    if (ret < 0) {
        LOGE("POLL error, %d", ret);
        return result-100;
    }
    if ((p.revents & (POLLERR | POLLHUP)) != 0) {
        LOGE("POLL event error, %d, %04X", ret, p.revents);
        return result-200;
    }
    result = read(fd, pBuffer, len);
    if (result < 0)
    {
        if ((result == EAGAIN) || (result == -1))
        {
            result = 0;
        }
        else {
            LOGE("Read error, %d", result);
        }

    }
    return result;
}

