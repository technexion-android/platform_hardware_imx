#pragma once

#include <trusty/tipc.h>
#define OEMCRYPTO_PORT "com.android.trusty.oemcrypto"
#define OEMCRYPTO_SECURE_PORT "com.android.trusty.oemcrypto.secure"
#define OEMCRYPTO_MAX_BUFFER_LENGTH 4096

enum oemcrypto_command : uint32_t{
    OEMCRYPTO_RESP_BIT = 1,
    OEMCRYPTO_STOP_BIT = 2,
    OEMCRYPTO_REQ_SHIFT = 2,
    OEMCRYPTO_RSADERIVEKEYS = 2<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_REFRESHKEY = 3<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_NONCECOLLISION = 4<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_INSTALLKEYBOX = 5<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_USETESTKEYBOX = 6<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_DEVICEROOTID = 7<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_TERMINATE = 8<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_VALIDATEKEYBOX = 9<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_GETOPENSESSIONNUM = 10<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_GETMAXNUMOFSESSION = 11<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_DEVICEROOTTOKENLEN = 12<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_NONCE_FLOOD_COUNT = 13<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_MAX_USAGETABLE_SIZE = 14<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_DEVICEROOTTOKEN = 15<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_CURRENT_HDCP_CAP = 16<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_INITIALIZE = 17<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_VALIDROOTOFTRUST = 18<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_LOADTESTRSAKEY = 19<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_CREATEUSAGETABLEHEADER = 20<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_LOADUSAGETABLEHEADER = 21<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_SHRINKUSAGETABLEHEADER = 22<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_PREPANDSIGNLICENSEREQUEST = 23<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_PREPANDSIGNRENEWALREQUEST = 24<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_LOADLICENSE = 25<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_LOADRENEWAL = 26<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_SIGNPROVISIONINGREQUEST = 27<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_GENERIC_ENCRYPT = 28<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_GENERIC_DECRYPT = 29<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_GENERIC_SIGN = 30<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_GENERIC_VERIFY = 31<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_VALIDATAMESSGE = 32<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_QUERYKEYCONTROLBLOCK = 33<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_SELECTCONTENTKEY = 34<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_DEACTIVEUSAGEENTRY = 35<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_REPORTUSAGE = 36<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_LOADUSAGEENTRY = 37<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_SETNONCE = 38<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_CHECKNONCE = 39<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_CREATENEWUSAGEENTRY = 40<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_MOVEENTRY = 41<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_UPDATEUSAGEENTRY = 42<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_GENERATERSASIGNATURE = 43<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_RSASIGNATURESIZE = 44<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_LOADENTITLEDCONTENTKEYS = 45<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_LOADKEYS = 46<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_DECRYPTSAMPLE = 47<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_ALLOWEDSCHEMEMS = 48<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_SUPPORTUSAGETABLE = 49<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_SETDECRYPTHASH = 50<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_GETHASHERRORCODE = 51<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_CRYPTOENGINE_COPYBUFFER = 52<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_DERIVEKEYS_1 = 53<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_LOADDRMPRIVATEKEY = 54<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_REWRAPDEVICERSAKEY = 55<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_ENABLE_G2D_SECURE_MODE = 56<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_DISABLE_G2D_SECURE_MODE = 57<<OEMCRYPTO_REQ_SHIFT,
    OEMCRYPTO_G2D_SECURE_MODE = 58<<OEMCRYPTO_REQ_SHIFT
};
struct oemcrypto_message {
    int  cmd;
    uint32_t session_id;
    uint8_t payload[0];
};

const uint32_t TRUSTY_OEMCRYPTO_RECV_BUF_SIZE = 2 * PAGE_SIZE;
const uint32_t TRUSTY_OEMCRYPTO_SEND_BUF_SIZE =
        (PAGE_SIZE - sizeof(struct oemcrypto_message) - 16 /* tipc header */);
