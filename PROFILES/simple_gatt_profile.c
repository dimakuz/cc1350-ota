/******************************************************************************

 @file  simple_gatt_profile.c

 @brief This file contains the Simple GATT profile sample GATT service profile
        for use with the BLE sample application.

 Group: WCS, BTS
 Target Device: CC1350

 ******************************************************************************
 
 Copyright (c) 2010-2017, Texas Instruments Incorporated
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

 *  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

 *  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 *  Neither the name of Texas Instruments Incorporated nor the names of
    its contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 ******************************************************************************
 Release Name: ti-ble-2.3.1-stack-sdk_1_60_xx
 Release Date: 2017-12-16 12:03:51
 *****************************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include <string.h>

#include "bcomdef.h"
#include "osal.h"
#include "linkdb.h"
#include "att.h"
#include "gatt.h"
#include "gatt_uuid.h"
#include "gattservapp.h"
#include "gapbondmgr.h"

#include "simple_gatt_profile.h"
#include <driverlib/sys_ctrl.h>
#include <Include/ota.h>

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

#define SERVAPP_NUM_ATTR_SUPPORTED        17

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

static const uint32_t OTA_BLOB_MAGIC = 0xdabad000;

#define OTA_MAX_BLOB_SIZE 400u
#define OTA_CHUNK_MTU     80u

#pragma pack(push, 1) // no padding
struct OTABlob
{
    uint32_t magic;
    uint16_t total_size;
    uint8_t cur_chunk;
    uint8_t num_chunks;
    uint16_t checksum;
    uint16_t chunk_len;
    /* uint8_t blob[0]; */
};

struct OTAHeader {
    uint16_t entrypoint;
    uint16_t size;
    struct ota_load loads[OTA_MAX_LOADS];
};

#pragma pack(pop)

static int g_previous_chunk = -1;
static unsigned g_num_bytes_rcvd;

extern int cur_chunk;
extern int previous_chunk;

static int check_blob(struct OTABlob* blob, size_t len)
{
    if (len < sizeof(struct OTABlob)) {
        //std::cerr << "struct is too small." << std::endl;
        while (1);
        return -1;
    }

    if (blob->magic != OTA_BLOB_MAGIC) {
        //std::cerr << "magic mismatch." << std::endl;
        while (1);
        return -1;
    }

    if (blob->total_size == 0 ||
        blob->total_size > OTA_MAX_BLOB_SIZE) {
        //std::cerr << "blob total size is 0 or exceeded max size: "
        //          << blob->chunk_len << std::endl;
        while (1);
        return -1;
    }

    if (blob->chunk_len == 0 ||
        blob->chunk_len > OTA_CHUNK_MTU) {
        //std::cerr << "chunk length is 0 or exceeded max size: "
        //          << blob->chunk_len << std::endl;
        while (1);
        return -1;
    }

    if (len < sizeof(*blob) + blob->chunk_len) {
        //std::cerr << "invalid blob size: "
        //          << g_num_bytes_rcvd + blob->chunk_len << std::endl;
        while (1);
        return -1;
    }

    // TODO: this doesn't allow interrupting a transfer
    //       (i.e. restarting a failed transfer).
    cur_chunk = (int)blob->cur_chunk;
    previous_chunk = g_previous_chunk + 1;
    if ((int)blob->cur_chunk != g_previous_chunk + 1) {
        //std::cerr << "invalid chunk number." << std::endl;
        while (1);
        return -1;
    }

    if (g_num_bytes_rcvd + blob->chunk_len > OTA_MAX_BLOB_SIZE) {
        //std::cerr << "exceeding max transfer with: "
        //          << g_num_bytes_rcvd + blob->chunk_len << std::endl;
        while (1);
        return -1;
    }

    return 0;
}


// Simple GATT Profile Service UUID: 0xFFF0
CONST uint8 simpleProfileServUUID[ATT_BT_UUID_SIZE] =
{ 
  LO_UINT16(SIMPLEPROFILE_SERV_UUID), HI_UINT16(SIMPLEPROFILE_SERV_UUID)
};

// Characteristic 1 UUID: 0xFFF1
CONST uint8 simpleProfilechar1UUID[ATT_BT_UUID_SIZE] =
{ 
  LO_UINT16(SIMPLEPROFILE_CHAR1_UUID), HI_UINT16(SIMPLEPROFILE_CHAR1_UUID)
};

// Characteristic 2 UUID: 0xFFF2
CONST uint8 simpleProfilechar2UUID[ATT_BT_UUID_SIZE] =
{ 
  LO_UINT16(SIMPLEPROFILE_CHAR2_UUID), HI_UINT16(SIMPLEPROFILE_CHAR2_UUID)
};

// Characteristic 3 UUID: 0xFFF3
CONST uint8 simpleProfilechar3UUID[ATT_BT_UUID_SIZE] =
{ 
  LO_UINT16(SIMPLEPROFILE_CHAR3_UUID), HI_UINT16(SIMPLEPROFILE_CHAR3_UUID)
};

// Characteristic 4 UUID: 0xFFF4
CONST uint8 simpleProfilechar4UUID[ATT_BT_UUID_SIZE] =
{ 
  LO_UINT16(SIMPLEPROFILE_CHAR4_UUID), HI_UINT16(SIMPLEPROFILE_CHAR4_UUID)
};

// Characteristic 5 UUID: 0xFFF5
CONST uint8 simpleProfilechar5UUID[ATT_BT_UUID_SIZE] =
{ 
  LO_UINT16(SIMPLEPROFILE_CHAR5_UUID), HI_UINT16(SIMPLEPROFILE_CHAR5_UUID)
};



/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

static simpleProfileCBs_t *simpleProfile_AppCBs = NULL;

/*********************************************************************
 * Profile Attributes - variables
 */

// Simple Profile Service attribute
static CONST gattAttrType_t simpleProfileService = { ATT_BT_UUID_SIZE, simpleProfileServUUID };


// Simple Profile Characteristic 1 Properties
static uint8 simpleProfileChar1Props = GATT_PROP_READ | GATT_PROP_WRITE;

// Characteristic 1 Value
static uint8 simpleProfileChar1 = 0;

// Simple Profile Characteristic 1 User Description
static uint8 simpleProfileChar1UserDesp[17] = "Characteristic 1";


// Simple Profile Characteristic 2 Properties
static uint8 simpleProfileChar2Props = GATT_PROP_READ;

// Characteristic 2 Value
static uint8 simpleProfileChar2 = 0;

// Simple Profile Characteristic 2 User Description
static uint8 simpleProfileChar2UserDesp[17] = "Characteristic 2";


// Simple Profile Characteristic 3 Properties
static uint8 simpleProfileChar3Props = GATT_PROP_WRITE;

// Characteristic 3 Value
static uint8 simpleProfileChar3[SIMPLEPROFILE_CHAR3_LEN] = { 0, };

static unsigned simpleProfileChar3ActualSize = SIMPLEPROFILE_CHAR3_LEN;

// Characteristic 3 Value
//static uint8 simpleProfileChar3 = 0;

// Simple Profile Characteristic 3 User Description
static uint8 simpleProfileChar3UserDesp[17] = "Characteristic 3";


// Simple Profile Characteristic 4 Properties
// Ofir
//static uint8 simpleProfileChar4Props = GATT_PROP_NOTIFY;
static uint8 simpleProfileChar4Props = GATT_PROP_READ | GATT_PROP_NOTIFY;


// Characteristic 4 Value
static uint8 simpleProfileChar4 = 0;

// Simple Profile Characteristic 4 Configuration Each client has its own
// instantiation of the Client Characteristic Configuration. Reads of the
// Client Characteristic Configuration only shows the configuration for
// that client and writes only affect the configuration of that client.
static gattCharCfg_t *simpleProfileChar4Config;
                                        
// Simple Profile Characteristic 4 User Description
static uint8 simpleProfileChar4UserDesp[17] = "Characteristic 4";


// Simple Profile Characteristic 5 Properties
static uint8 simpleProfileChar5Props = GATT_PROP_READ;

// Characteristic 5 Value
static uint8 simpleProfileChar5[SIMPLEPROFILE_CHAR5_LEN] = { 0, 0, 0, 0, 0 };

// Simple Profile Characteristic 5 User Description
static uint8 simpleProfileChar5UserDesp[17] = "Characteristic 5";

/*********************************************************************
 * Profile Attributes - Table
 */

static gattAttribute_t simpleProfileAttrTbl[SERVAPP_NUM_ATTR_SUPPORTED] = 
{
  // Simple Profile Service
  { 
    { ATT_BT_UUID_SIZE, primaryServiceUUID }, /* type */
    GATT_PERMIT_READ,                         /* permissions */
    0,                                        /* handle */
    (uint8 *)&simpleProfileService            /* pValue */
  },

    // Characteristic 1 Declaration
    { 
      { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ, 
      0,
      &simpleProfileChar1Props 
    },

      // Characteristic Value 1
      { 
        { ATT_BT_UUID_SIZE, simpleProfilechar1UUID },
        GATT_PERMIT_READ | GATT_PERMIT_WRITE, 
        0, 
        &simpleProfileChar1 
      },

      // Characteristic 1 User Description
      { 
        { ATT_BT_UUID_SIZE, charUserDescUUID },
        GATT_PERMIT_READ, 
        0, 
        simpleProfileChar1UserDesp 
      },      

    // Characteristic 2 Declaration
    { 
      { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ, 
      0,
      &simpleProfileChar2Props 
    },

      // Characteristic Value 2
      { 
        { ATT_BT_UUID_SIZE, simpleProfilechar2UUID },
        GATT_PERMIT_READ, 
        0, 
        &simpleProfileChar2 
      },

      // Characteristic 2 User Description
      { 
        { ATT_BT_UUID_SIZE, charUserDescUUID },
        GATT_PERMIT_READ, 
        0, 
        simpleProfileChar2UserDesp 
      },           
      
    // Characteristic 3 Declaration
    { 
      { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ, 
      0,
      &simpleProfileChar3Props 
    },

      // Characteristic Value 3
      { 
        { ATT_BT_UUID_SIZE, simpleProfilechar3UUID },
        GATT_PERMIT_READ | GATT_PERMIT_WRITE,
        0, 
        simpleProfileChar3
      },

      // Characteristic 3 User Description
      { 
        { ATT_BT_UUID_SIZE, charUserDescUUID },
        GATT_PERMIT_READ, 
        0, 
        simpleProfileChar3UserDesp 
      },

    // Characteristic 4 Declaration
    { 
      { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ, 
      0,
      &simpleProfileChar4Props 
    },

      // Characteristic Value 4
      { 
        { ATT_BT_UUID_SIZE, simpleProfilechar4UUID },
        //Ofir 0,
        GATT_PERMIT_READ,
        0, 
        &simpleProfileChar4 
      },

      // Characteristic 4 configuration
      { 
        { ATT_BT_UUID_SIZE, clientCharCfgUUID },
        GATT_PERMIT_READ | GATT_PERMIT_WRITE, 
        0, 
        (uint8 *)&simpleProfileChar4Config 
      },
      
      // Characteristic 4 User Description
      { 
        { ATT_BT_UUID_SIZE, charUserDescUUID },
        GATT_PERMIT_READ, 
        0, 
        simpleProfileChar4UserDesp 
      },
      
    // Characteristic 5 Declaration
    { 
      { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ, 
      0,
      &simpleProfileChar5Props 
    },

      // Characteristic Value 5
      { 
        { ATT_BT_UUID_SIZE, simpleProfilechar5UUID },
        GATT_PERMIT_AUTHEN_READ, 
        0, 
        simpleProfileChar5 
      },

      // Characteristic 5 User Description
      { 
        { ATT_BT_UUID_SIZE, charUserDescUUID },
        GATT_PERMIT_READ, 
        0, 
        simpleProfileChar5UserDesp 
      },
};

#define _OTA_STATE_NEW  0
#define _OTA_STATE_DATA 1
static int _ota_state = _OTA_STATE_NEW;
static struct ota_dl_params ota_params;
static struct ota_dl_state ota_state;
extern unsigned int bytes_received;

static int ota_transaction(struct OTABlob* blob, size_t len)
{
    if (check_blob(blob, len)) {
        return -1;
    }
    len -= sizeof (struct OTABlob);

    uint8_t *data = (void *) blob;
    data = data + sizeof (struct OTABlob);
    struct OTAHeader *header;

    switch (_ota_state) {
    case _OTA_STATE_NEW:
        header = (struct OTAHeader *) data;
        ota_dl_params_init(&ota_params);
        ota_params.entrypoint = (ota_entrypoint_t) header->entrypoint;
        ota_params.dl_size = header->size;
        for (int i = 0; i < OTA_MAX_LOADS; i++) {
            ota_params.loads[i].offset = header->loads[i].offset;
            ota_params.loads[i].len = header->loads[i].len;
            ota_params.loads[i].dest = header->loads[i].dest;
        }
        ota_dl_init(&ota_state, &ota_params);
        ota_dl_begin(&ota_state);
        len -= sizeof (struct OTAHeader);
        data = data + sizeof (struct OTAHeader);
        _ota_state = _OTA_STATE_DATA;
    case _OTA_STATE_DATA:
        if (len) {
            ota_dl_process(&ota_state, data, len);
        }
        if (ota_state.dl_done == ota_state.dl_size) {
            ota_dl_finish(&ota_state);
        }
        break;
    }

    g_previous_chunk++;
    g_num_bytes_rcvd += blob->chunk_len;
    bytes_received = g_num_bytes_rcvd;

    if (blob->cur_chunk + 1 == blob->num_chunks) {
        //std::cout << "done, received: " << g_num_bytes_rcvd
        //          << " bytes." << std::endl;
        g_previous_chunk = -1;
        g_num_bytes_rcvd = 0;
        // TODO: add crc check
    }

    return 0;
}


/*********************************************************************
 * LOCAL FUNCTIONS
 */
static bStatus_t simpleProfile_ReadAttrCB(uint16_t connHandle,
                                          gattAttribute_t *pAttr, 
                                          uint8_t *pValue, uint16_t *pLen,
                                          uint16_t offset, uint16_t maxLen,
                                          uint8_t method);
static bStatus_t simpleProfile_WriteAttrCB(uint16_t connHandle,
                                           gattAttribute_t *pAttr,
                                           uint8_t *pValue, uint16_t len,
                                           uint16_t offset, uint8_t method);

/*********************************************************************
 * PROFILE CALLBACKS
 */

// Simple Profile Service Callbacks
// Note: When an operation on a characteristic requires authorization and 
// pfnAuthorizeAttrCB is not defined for that characteristic's service, the 
// Stack will report a status of ATT_ERR_UNLIKELY to the client.  When an 
// operation on a characteristic requires authorization the Stack will call 
// pfnAuthorizeAttrCB to check a client's authorization prior to calling
// pfnReadAttrCB or pfnWriteAttrCB, so no checks for authorization need to be 
// made within these functions.
CONST gattServiceCBs_t simpleProfileCBs =
{
  simpleProfile_ReadAttrCB,  // Read callback function pointer
  simpleProfile_WriteAttrCB, // Write callback function pointer
  NULL                       // Authorization callback function pointer
};

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      SimpleProfile_AddService
 *
 * @brief   Initializes the Simple Profile service by registering
 *          GATT attributes with the GATT server.
 *
 * @param   services - services to add. This is a bit map and can
 *                     contain more than one service.
 *
 * @return  Success or Failure
 */
bStatus_t SimpleProfile_AddService( uint32 services )
{
  uint8 status;

  // Allocate Client Characteristic Configuration table
  simpleProfileChar4Config = (gattCharCfg_t *)ICall_malloc( sizeof(gattCharCfg_t) *
                                                            linkDBNumConns );
  if ( simpleProfileChar4Config == NULL )
  {     
    return ( bleMemAllocError );
  }
  
  // Initialize Client Characteristic Configuration attributes
  GATTServApp_InitCharCfg( INVALID_CONNHANDLE, simpleProfileChar4Config );
  
  if ( services & SIMPLEPROFILE_SERVICE )
  {
    // Register GATT attribute list and CBs with GATT Server App
    status = GATTServApp_RegisterService( simpleProfileAttrTbl, 
                                          GATT_NUM_ATTRS( simpleProfileAttrTbl ),
                                          GATT_MAX_ENCRYPT_KEY_SIZE,
                                          &simpleProfileCBs );
  }
  else
  {
    status = SUCCESS;
  }

  return ( status );
}

/*********************************************************************
 * @fn      SimpleProfile_RegisterAppCBs
 *
 * @brief   Registers the application callback function. Only call 
 *          this function once.
 *
 * @param   callbacks - pointer to application callbacks.
 *
 * @return  SUCCESS or bleAlreadyInRequestedMode
 */
bStatus_t SimpleProfile_RegisterAppCBs( simpleProfileCBs_t *appCallbacks )
{
  if ( appCallbacks )
  {
    simpleProfile_AppCBs = appCallbacks;
    
    return ( SUCCESS );
  }
  else
  {
    return ( bleAlreadyInRequestedMode );
  }
}

/*********************************************************************
 * @fn      SimpleProfile_SetParameter
 *
 * @brief   Set a Simple Profile parameter.
 *
 * @param   param - Profile parameter ID
 * @param   len - length of data to write
 * @param   value - pointer to data to write.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate 
 *          data type (example: data type of uint16 will be cast to 
 *          uint16 pointer).
 *
 * @return  bStatus_t
 */
bStatus_t SimpleProfile_SetParameter( uint8 param, uint8 len, void *value )
{
  bStatus_t ret = SUCCESS;
  switch ( param )
  {
    case SIMPLEPROFILE_CHAR1:
      if ( len == sizeof ( uint8 ) ) 
      {
        simpleProfileChar1 = *((uint8*)value);
      }
      else
      {
        ret = bleInvalidRange;
      }
      break;

    case SIMPLEPROFILE_CHAR2:
      if ( len == sizeof ( uint8 ) ) 
      {
        simpleProfileChar2 = *((uint8*)value);
      }
      else
      {
        ret = bleInvalidRange;
      }
      break;

    case SIMPLEPROFILE_CHAR3:
      if ( len <= SIMPLEPROFILE_CHAR3_LEN )
      {
        VOID memcpy( simpleProfileChar3, value, len );
      }
      else
      {
        ret = bleInvalidRange;
      }
      break;

    case SIMPLEPROFILE_CHAR4:
      if ( len == sizeof ( uint8 ) ) 
      {
        simpleProfileChar4 = *((uint8*)value);
        
        // See if Notification has been enabled
        GATTServApp_ProcessCharCfg( simpleProfileChar4Config, &simpleProfileChar4, FALSE,
                                    simpleProfileAttrTbl, GATT_NUM_ATTRS( simpleProfileAttrTbl ),
                                    INVALID_TASK_ID, simpleProfile_ReadAttrCB );
      }
      else
      {
        ret = bleInvalidRange;
      }
      break;

    case SIMPLEPROFILE_CHAR5:
      if ( len == SIMPLEPROFILE_CHAR5_LEN )
      {
        VOID memcpy( simpleProfileChar5, value, SIMPLEPROFILE_CHAR5_LEN );
      }
      else
      {
        ret = bleInvalidRange;
      }
      break;
      
    default:
      ret = INVALIDPARAMETER;
      break;
  }
  
  return ( ret );
}

/*********************************************************************
 * @fn      SimpleProfile_GetParameter
 *
 * @brief   Get a Simple Profile parameter.
 *
 * @param   param - Profile parameter ID
 * @param   value - pointer to data to put.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate 
 *          data type (example: data type of uint16 will be cast to 
 *          uint16 pointer).
 *
 * @return  bStatus_t
 */
bStatus_t SimpleProfile_GetParameter( uint8 param, void *value )
{
  bStatus_t ret = SUCCESS;
  switch ( param )
  {
    case SIMPLEPROFILE_CHAR1:
      *((uint8*)value) = simpleProfileChar1;
      break;

    case SIMPLEPROFILE_CHAR2:
      *((uint8*)value) = simpleProfileChar2;
      break;      

    case SIMPLEPROFILE_CHAR3:
      *((uint8*)value) = simpleProfileChar3;
      break;  

    case SIMPLEPROFILE_CHAR4:
      *((uint8*)value) = simpleProfileChar4;
      break;

    case SIMPLEPROFILE_CHAR5:
      VOID memcpy( value, simpleProfileChar5, SIMPLEPROFILE_CHAR5_LEN );
      break;      
      
    default:
      ret = INVALIDPARAMETER;
      break;
  }
  
  return ( ret );
}

/*********************************************************************
 * @fn          simpleProfile_ReadAttrCB
 *
 * @brief       Read an attribute.
 *
 * @param       connHandle - connection message was received on
 * @param       pAttr - pointer to attribute
 * @param       pValue - pointer to data to be read
 * @param       pLen - length of data to be read
 * @param       offset - offset of the first octet to be read
 * @param       maxLen - maximum length of data to be read
 * @param       method - type of read message
 *
 * @return      SUCCESS, blePending or Failure
 */
static bStatus_t simpleProfile_ReadAttrCB(uint16_t connHandle,
                                          gattAttribute_t *pAttr,
                                          uint8_t *pValue, uint16_t *pLen,
                                          uint16_t offset, uint16_t maxLen,
                                          uint8_t method)
{
  bStatus_t status = SUCCESS;
  
  // Make sure it's not a blob operation (no attributes in the profile are long)
  if ( offset > 0 )
  {
    return ( ATT_ERR_ATTR_NOT_LONG );
  }
 
  if ( pAttr->type.len == ATT_BT_UUID_SIZE )
  {
    // 16-bit UUID
    uint16 uuid = BUILD_UINT16( pAttr->type.uuid[0], pAttr->type.uuid[1]);
    switch ( uuid )
    {
      // No need for "GATT_SERVICE_UUID" or "GATT_CLIENT_CHAR_CFG_UUID" cases;
      // gattserverapp handles those reads

      // characteristics 1 and 2 have read permissions
      // characteritisc 3 does not have read permissions; therefore it is not
      //   included here
      // characteristic 4 does not have read permissions, but because it
      //   can be sent as a notification, it is included here
      case SIMPLEPROFILE_CHAR1_UUID:
      case SIMPLEPROFILE_CHAR2_UUID:
      case SIMPLEPROFILE_CHAR4_UUID:
        *pLen = 1;
        pValue[0] = *pAttr->pValue;
        break;

      case SIMPLEPROFILE_CHAR5_UUID:
        *pLen = SIMPLEPROFILE_CHAR5_LEN;
        VOID memcpy( pValue, pAttr->pValue, SIMPLEPROFILE_CHAR5_LEN );
        break;
        
      case SIMPLEPROFILE_CHAR3_UUID:
        *pLen = simpleProfileChar3ActualSize;
        VOID memcpy( pValue, pAttr->pValue, simpleProfileChar3ActualSize );
        break;

      default:
        // Should never get here! (characteristics 3 and 4 do not have read permissions)
        *pLen = 0;
        status = ATT_ERR_ATTR_NOT_FOUND;
        break;
    }
  }
  else
  {
    // 128-bit UUID
    *pLen = 0;
    status = ATT_ERR_INVALID_HANDLE;
  }

  return ( status );
}

extern void* my_dst_ptr;
extern int cur_ret;

/*********************************************************************
 * @fn      simpleProfile_WriteAttrCB
 *
 * @brief   Validate attribute data prior to a write operation
 *
 * @param   connHandle - connection message was received on
 * @param   pAttr - pointer to attribute
 * @param   pValue - pointer to data to be written
 * @param   len - length of data
 * @param   offset - offset of the first octet to be written
 * @param   method - type of write message
 *
 * @return  SUCCESS, blePending or Failure
 */
static bStatus_t simpleProfile_WriteAttrCB(uint16_t connHandle,
                                           gattAttribute_t *pAttr,
                                           uint8_t *pValue, uint16_t len,
                                           uint16_t offset, uint8_t method)
{
  bStatus_t status = SUCCESS;
  uint8 notifyApp = 0xFF;
  
  if ( pAttr->type.len == ATT_BT_UUID_SIZE )
  {
    // 16-bit UUID
    uint16 uuid = BUILD_UINT16( pAttr->type.uuid[0], pAttr->type.uuid[1]);
    switch ( uuid )
    {
      case SIMPLEPROFILE_CHAR1_UUID:

          //Validate the value
          // Make sure it's not a blob oper
          if ( offset == 0 )
          {
            if ( len != 1 )
            {
              status = ATT_ERR_INVALID_VALUE_SIZE;
            }
          }
          else
          {
            status = ATT_ERR_ATTR_NOT_LONG;
          }

          //Write the value
          if ( status == SUCCESS )
          {
            uint8 *pCurValue = (uint8 *)pAttr->pValue;
            *pCurValue = pValue[0];

            if( pAttr->pValue == &simpleProfileChar1 )
            {
              notifyApp = SIMPLEPROFILE_CHAR1;
            }
            else
            {
              notifyApp = SIMPLEPROFILE_CHAR3;
            }
          }

          break;

      case SIMPLEPROFILE_CHAR3_UUID:
        my_dst_ptr = simpleProfileChar3;

        //Validate the value
        if ( offset != 0 || len > SIMPLEPROFILE_CHAR3_LEN ||
             ota_transaction((struct OTABlob*)pValue, len) )
        {
            status = ATT_ERR_INVALID_VALUE_SIZE;
            while (1);
        }

        //Write the value
        if ( status == SUCCESS )
        {
          /* we finished copying the entire blob */
          if (g_previous_chunk == -1) {
             SysCtrlSystemReset();
          }
        }
        break;

      case GATT_CLIENT_CHAR_CFG_UUID:
        status = GATTServApp_ProcessCCCWriteReq( connHandle, pAttr, pValue, len,
                                                 offset, GATT_CLIENT_CFG_NOTIFY );
        break;
        
      default:
        // Should never get here! (characteristics 2 and 4 do not have write permissions)
        status = ATT_ERR_ATTR_NOT_FOUND;
        break;
    }
  }
  else
  {
    // 128-bit UUID
    status = ATT_ERR_INVALID_HANDLE;
  }

  // If a characteristic value changed then callback function to notify application of change
  if ( (notifyApp != 0xFF ) && simpleProfile_AppCBs && simpleProfile_AppCBs->pfnSimpleProfileChange )
  {
    simpleProfile_AppCBs->pfnSimpleProfileChange( notifyApp );  
  }
  
  return ( status );
}

/*********************************************************************
*********************************************************************/
