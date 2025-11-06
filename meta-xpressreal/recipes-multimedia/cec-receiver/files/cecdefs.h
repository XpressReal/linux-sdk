#ifndef _CECDEFS_H_
#define _CECDEFS_H_

#include <pthread.h>
#include <stdint.h>

typedef enum {                                     
    CEC_VERSION_1_1                         = 0x00,
    CEC_VERSION_1_2                         = 0x01,
    CEC_VERSION_1_2A                        = 0x02,
    CEC_VERSION_1_3                         = 0x03,
    CEC_VERSION_1_3A                        = 0x04,
    CEC_VERSION_1_4                         = 0x05,
    CEC_VERSION_UNKNOWN                     = 0xFF,
}CEC_VERSION;                                      

typedef enum cec_logical_address {
    CEC_ADDR_TV = 0,
    CEC_ADDR_RECORDER_1 = 1,
    CEC_ADDR_RECORDER_2 = 2,
    CEC_ADDR_TUNER_1 = 3,
    CEC_ADDR_PLAYBACK_1 = 4,
    CEC_ADDR_AUDIO_SYSTEM = 5,
    CEC_ADDR_TUNER_2 = 6,
    CEC_ADDR_TUNER_3 = 7,
    CEC_ADDR_PLAYBACK_2 = 8,
    CEC_ADDR_RECORDER_3 = 9,
    CEC_ADDR_TUNER_4 = 10,
    CEC_ADDR_PLAYBACK_3 = 11,
    CEC_ADDR_RESERVED_1 = 12,
    CEC_ADDR_RESERVED_2 = 13,
    CEC_ADDR_FREE_USE = 14,
    CEC_ADDR_UNREGISTERED = 15,
    CEC_ADDR_BROADCAST = 15
} cec_logical_address_t;

typedef enum cec_user_control_code
{
  CEC_USER_CONTROL_CODE_SELECT                      = 0x00,
  CEC_USER_CONTROL_CODE_UP                          = 0x01,
  CEC_USER_CONTROL_CODE_DOWN                        = 0x02,
  CEC_USER_CONTROL_CODE_LEFT                        = 0x03,
  CEC_USER_CONTROL_CODE_RIGHT                       = 0x04,
  CEC_USER_CONTROL_CODE_RIGHT_UP                    = 0x05,
  CEC_USER_CONTROL_CODE_RIGHT_DOWN                  = 0x06,
  CEC_USER_CONTROL_CODE_LEFT_UP                     = 0x07,
  CEC_USER_CONTROL_CODE_LEFT_DOWN                   = 0x08,
  CEC_USER_CONTROL_CODE_ROOT_MENU                   = 0x09,
  CEC_USER_CONTROL_CODE_SETUP_MENU                  = 0x0A,
  CEC_USER_CONTROL_CODE_CONTENTS_MENU               = 0x0B,
  CEC_USER_CONTROL_CODE_FAVORITE_MENU               = 0x0C,
  CEC_USER_CONTROL_CODE_EXIT                        = 0x0D,
  // reserved: 0x0E, 0x0F
  CEC_USER_CONTROL_CODE_TOP_MENU                    = 0x10,
  CEC_USER_CONTROL_CODE_DVD_MENU                    = 0x11,
  // reserved: 0x12 ... 0x1C
  CEC_USER_CONTROL_CODE_NUMBER_ENTRY_MODE           = 0x1D,
  CEC_USER_CONTROL_CODE_NUMBER11                    = 0x1E,
  CEC_USER_CONTROL_CODE_NUMBER12                    = 0x1F,
  CEC_USER_CONTROL_CODE_NUMBER0                     = 0x20,
  CEC_USER_CONTROL_CODE_NUMBER1                     = 0x21,
  CEC_USER_CONTROL_CODE_NUMBER2                     = 0x22,
  CEC_USER_CONTROL_CODE_NUMBER3                     = 0x23,
  CEC_USER_CONTROL_CODE_NUMBER4                     = 0x24,
  CEC_USER_CONTROL_CODE_NUMBER5                     = 0x25,
  CEC_USER_CONTROL_CODE_NUMBER6                     = 0x26,
  CEC_USER_CONTROL_CODE_NUMBER7                     = 0x27,
  CEC_USER_CONTROL_CODE_NUMBER8                     = 0x28,
  CEC_USER_CONTROL_CODE_NUMBER9                     = 0x29,
  CEC_USER_CONTROL_CODE_DOT                         = 0x2A,
  CEC_USER_CONTROL_CODE_ENTER                       = 0x2B,
  CEC_USER_CONTROL_CODE_CLEAR                       = 0x2C,
  CEC_USER_CONTROL_CODE_NEXT_FAVORITE               = 0x2F,
  CEC_USER_CONTROL_CODE_CHANNEL_UP                  = 0x30,
  CEC_USER_CONTROL_CODE_CHANNEL_DOWN                = 0x31,
  CEC_USER_CONTROL_CODE_PREVIOUS_CHANNEL            = 0x32,
  CEC_USER_CONTROL_CODE_SOUND_SELECT                = 0x33,
  CEC_USER_CONTROL_CODE_INPUT_SELECT                = 0x34,
  CEC_USER_CONTROL_CODE_DISPLAY_INFORMATION         = 0x35,
  CEC_USER_CONTROL_CODE_HELP                        = 0x36,
  CEC_USER_CONTROL_CODE_PAGE_UP                     = 0x37,
  CEC_USER_CONTROL_CODE_PAGE_DOWN                   = 0x38,
  // reserved: 0x39 ... 0x3F
  CEC_USER_CONTROL_CODE_POWER                       = 0x40,
  CEC_USER_CONTROL_CODE_VOLUME_UP                   = 0x41,
  CEC_USER_CONTROL_CODE_VOLUME_DOWN                 = 0x42,
  CEC_USER_CONTROL_CODE_MUTE                        = 0x43,
  CEC_USER_CONTROL_CODE_PLAY                        = 0x44,
  CEC_USER_CONTROL_CODE_STOP                        = 0x45,
  CEC_USER_CONTROL_CODE_PAUSE                       = 0x46,
  CEC_USER_CONTROL_CODE_RECORD                      = 0x47,
  CEC_USER_CONTROL_CODE_REWIND                      = 0x48,
  CEC_USER_CONTROL_CODE_FAST_FORWARD                = 0x49,
  CEC_USER_CONTROL_CODE_EJECT                       = 0x4A,
  CEC_USER_CONTROL_CODE_FORWARD                     = 0x4B,
  CEC_USER_CONTROL_CODE_BACKWARD                    = 0x4C,
  CEC_USER_CONTROL_CODE_STOP_RECORD                 = 0x4D,
  CEC_USER_CONTROL_CODE_PAUSE_RECORD                = 0x4E,
  // reserved: 0x4F
  CEC_USER_CONTROL_CODE_ANGLE                       = 0x50,
  CEC_USER_CONTROL_CODE_SUB_PICTURE                 = 0x51,
  CEC_USER_CONTROL_CODE_VIDEO_ON_DEMAND             = 0x52,
  CEC_USER_CONTROL_CODE_ELECTRONIC_PROGRAM_GUIDE    = 0x53,
  CEC_USER_CONTROL_CODE_TIMER_PROGRAMMING           = 0x54,
  CEC_USER_CONTROL_CODE_INITIAL_CONFIGURATION       = 0x55,
  CEC_USER_CONTROL_CODE_SELECT_BROADCAST_TYPE       = 0x56,
  CEC_USER_CONTROL_CODE_SELECT_SOUND_PRESENTATION   = 0x57,
  // reserved: 0x58 ... 0x5F
  CEC_USER_CONTROL_CODE_PLAY_FUNCTION               = 0x60,
  CEC_USER_CONTROL_CODE_PAUSE_PLAY_FUNCTION         = 0x61,
  CEC_USER_CONTROL_CODE_RECORD_FUNCTION             = 0x62,
  CEC_USER_CONTROL_CODE_PAUSE_RECORD_FUNCTION       = 0x63,
  CEC_USER_CONTROL_CODE_STOP_FUNCTION               = 0x64,
  CEC_USER_CONTROL_CODE_MUTE_FUNCTION               = 0x65,
  CEC_USER_CONTROL_CODE_RESTORE_VOLUME_FUNCTION     = 0x66,
  CEC_USER_CONTROL_CODE_TUNE_FUNCTION               = 0x67,
  CEC_USER_CONTROL_CODE_SELECT_MEDIA_FUNCTION       = 0x68,
  CEC_USER_CONTROL_CODE_SELECT_AV_INPUT_FUNCTION    = 0x69,
  CEC_USER_CONTROL_CODE_SELECT_AUDIO_INPUT_FUNCTION = 0x6A,
  CEC_USER_CONTROL_CODE_POWER_TOGGLE_FUNCTION       = 0x6B,
  CEC_USER_CONTROL_CODE_POWER_OFF_FUNCTION          = 0x6C,
  CEC_USER_CONTROL_CODE_POWER_ON_FUNCTION           = 0x6D,
  // reserved: 0x6E ... 0x70
  CEC_USER_CONTROL_CODE_F1_BLUE                     = 0x71,
  CEC_USER_CONTROL_CODE_F2_RED                      = 0X72,
  CEC_USER_CONTROL_CODE_F3_GREEN                    = 0x73,
  CEC_USER_CONTROL_CODE_F4_YELLOW                   = 0x74,
  CEC_USER_CONTROL_CODE_F5                          = 0x75,
  CEC_USER_CONTROL_CODE_DATA                        = 0x76,
  // reserved: 0x77 ... 0xFF
  CEC_USER_CONTROL_CODE_AN_RETURN                   = 0x91, // return (Samsung)
  CEC_USER_CONTROL_CODE_AN_CHANNELS_LIST            = 0x96, // channels list (Samsung)
  CEC_USER_CONTROL_CODE_MAX                         = 0x96,
  CEC_USER_CONTROL_CODE_UNKNOWN                     = 0xFF
} cec_user_control_code;

typedef enum 
{    
    //One Touch Play	    
    CEC_IMAGE_VIEW_ON                   = 0x04,	 
    CEC_TEXT_VIEW_ON                    = 0x0D,	
	                                    
    //Routing Control	                
    CEC_ACTIVE_SOURCE                   = 0x82, 
    CEC_INACTIVE_SOURCE                 = 0x9D, // <HDMI 1.3a>
    CEC_REQUEST_ACTIVE_SOURCE           = 0x85,	
    CEC_SET_STREAM_PATH                 = 0x86,	
    CEC_ROUTING_CHANGE                  = 0x80,	
    CEC_ROUTING_INFORMATION             = 0x81,
	                                    
    //System Standby	                
    CEC_STANDBY                         = 0x36, 
	                                    
    //One Touch Record	                
    CEC_RECORD_ON                       = 0x09,	
    CEC_RECORD_OFF                      = 0x0B,	
    CEC_RECORD_STATUS                   = 0x0A,	
    CEC_RECORD_TV_SCREEN                = 0x0F,	
	                                    
	//Timer Programming	                
	CEC_CLEAR_ANALOGUE_TIMER            = 0x33, // <HDMI 1.3a>
    CEC_CLEAR_DIGITAL_TIMER             = 0x99, // <HDMI 1.3a>
    CEC_CLEAR_EXTERNAL_TIMER            = 0xA1, // <HDMI 1.3a>   
    CEC_SET_ANALOGUE_TIMER              = 0x34, // <HDMI 1.3a>
    CEC_SET_DIGITAL_TIMER               = 0x97, // <HDMI 1.3a>
    CEC_SET_EXTERNAL_TIMER              = 0xA2, // <HDMI 1.3a>
    CEC_SET_TIMER_PROGRAM_TITLE         = 0x67, // <HDMI 1.3a>
    CEC_TIMER_CLEARED_STATUS            = 0x43, // <HDMI 1.3a>
    CEC_TIMER_STATUS                    = 0x35, // <HDMI 1.3a> 
	                                    
    //System Information	            
    CEC_CEC_VERSION                     = 0x9E, // <HDMI 1.3a>     
	CEC_GET_CEC_VERSION                 = 0x9F, // <HDMI 1.3a>
	CEC_GET_MENU_LANGUAGE               = 0x91, 
	CEC_SET_MENU_LANGUAGE               = 0x32,
	CEC_GIVE_PHYSICAL_ADDRESS           = 0x83,
	CEC_REPORT_PHYSICAL_ADDRESS         = 0x84,    
	                                    
    //Deck Control	                    
    CEC_GIVE_DECK_STATUS                = 0x1A,	
    CEC_DECK_STATUS                     = 0x1B,	
    CEC_DECK_CONTROL                    = 0x42,	
    CEC_PLAY                            = 0x41,	
	                                    
    //Tuner Control	                    
    CEC_GIVE_TUNER_DEVICE_STATUS        = 0x08,	
    CEC_TUNER_DEVICE_STATUS             = 0x07,	
    CEC_SELECT_ANALOGUE_SERVICE         = 0x92,	// <HDMI 1.3a>
    CEC_SELECT_DIGITAL_SERVICE          = 0x93,	
    CEC_TUNER_STEP_DECREMENT            = 0x06,	
    CEC_TUNER_STEP_INCREMENT            = 0x05,	
	                                    
    //Vendor Specific Command	        
	CEC_DEVICE_VENDOR_ID                = 0x87,
	CEC_GIVE_DEVICE_VENDOR_ID           = 0x8C,
	CEC_VENDOR_COMMAND                  = 0x89,
	CEC_VENDOR_COMMAND_WITH_ID          = 0xA0,	// <HDMI 1.3a>
	CEC_VENDOR_REMOTE_BUTTON_DOWN       = 0x8A,
	CEC_VENDOR_REMOTE_BUTTON_UP         = 0x8B,
	                                    
    //OSD Status Display	            
    CEC_SET_OSD_STRING                  = 0x64,
	                                    
    //OSD Status Display	            
    CEC_GIVE_OSD_NAME                   = 0x46,	
    CEC_SET_OSD_NAME                    = 0x47,
	                                    
    //Device Menu Control	            
    CEC_MENU_REQUEST                    = 0x8D,	
    CEC_MENU_STATUS                     = 0x8E,	
	                                    
    //Remote Control Pass Through	    
    CEC_USER_CONTROL_PRESSED            = 0x44,
    CEC_USER_CONTROL_RELEASED           = 0x45,
	                                    
    //Power Status Feature	            
    CEC_GIVE_DEVICE_POWER_STATUS        = 0x8F,
    CEC_REPORT_POWER_STATUS             = 0x90,
	                                    
    //General  Protocol 	            
    CEC_FEATURE_ABORT                   = 0x00,
    CEC_ABORT                           = 0xFF,
                                        
    //System Audio Control              
    CEC_GIVE_AUDIO_STATUS               = 0x71, // <HDMI 1.3a>
    CEC_GIVE_SYSTEM_AUDIO_MODE_STATUS   = 0x7D, // <HDMI 1.3a>   
    CEC_REPORT_AUDIO_STATUS             = 0x7A, // <HDMI 1.3a>
    CEC_SET_SYSTEM_AUDIO_MODE           = 0x72, // <HDMI 1.3a>
    CEC_SYSTEM_AUDIO_MODE_REQUEST       = 0x70, // <HDMI 1.3a>
    CEC_SYSTEM_AUDIO_MODE_STATUS        = 0x7E, // <HDMI 1.3a>
                                                             
    // Audio Rate Control                                    
    CEC_SET_AUDIO_RATE                  = 0x9A, // <HDMI 1.3a>
    
    // Audio Return channel
    CEC_INITIATE_ARC                    = 0xC0, // <HDMI 1.4>
    CEC_REPORT_ARC_INITIATED            = 0xC1, // <HDMI 1.4>
    CEC_REPORT_ARC_TERMINATED           = 0xC2, // <HDMI 1.4>
    CEC_REQUEST_ARC_INITIATION          = 0xC3, // <HDMI 1.4>
    CEC_REQUEST_ARC_TERMINATION         = 0xC4, // <HDMI 1.4>
    CEC_TERMINATE_ARC                   = 0xC5, // <HDMI 1.4>
    
    // Capability Discovery and Control 
    CEC_CDC_MESSAGE                     = 0xF8, // <HDMI 1.4>

} cec_opcode;

typedef struct _cec_keypress
{
  cec_user_control_code keycode;   /**< the keycode */
  unsigned int          duration;  /**< the duration of the keypress */
} cec_keypress;

typedef struct _cec_callback
{
    void (*key_press)(void *cbparam, const cec_keypress *key);
    void (*cmd_received)(void *cbparam, void *cmd); 
} cec_callback_t;

typedef struct _cec_adapter
{    
    int                  fd;
    uint8_t              monitor;
    uint16_t             physicalAddr;
    uint16_t             logicalAddr;
    uint16_t             prim_devtype;
    pthread_t            cec_tid;
    const char           *device;
    struct cec_log_addrs *log_addrs;
    struct cec_caps      *caps;
    struct cec_msg       *msg;
    cec_callback_t       *callbacks;
} cec_adapter_t;

#endif /* _CECDEFS_H_ */
