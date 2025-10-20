#ifndef CTRL_SERVICE_H
#define CTRL_SERVICE_H

int accept_request();

enum {
    JOIN_REQUEST_CODE = 1,
    RECV_NODEDATA_LIST_REQUEST_CODE = 2,
};

#endif /* CTRL_SERVICE_H */
