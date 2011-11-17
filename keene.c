#include <stdio.h>
#include <string.h>
#include <usb.h>
#include <math.h>
#include <errno.h>

extern int errnol;
usb_dev_handle  *keenehandle;

#define keeneVID  0x046d
#define keenePID  0x0a0e

// ** cleanup() *****************************************************
void cleanup() {
    usb_set_debug(0);
    // if we have the USB device, hand it back
    if (keenehandle) {
        usb_release_interface(keenehandle,2);
    }
    usb_close(keenehandle);
    exit(errno);
}

usb_dev_handle * GetFirstDevice(int vendorid, int productid) {
  struct usb_bus    *bus;
  struct usb_device *dev;

  usb_dev_handle    *device_handle = NULL; // it's a null

  usb_init();
  usb_set_debug(0);     /* 3 is the max setting */

  usb_find_busses();
  usb_find_devices();
 
  // Loop through each bus and device until you find the first match
  // open it and return it's usb device handle

  for (bus = usb_busses; bus; bus = bus->next) {
    for (dev = bus->devices; dev; dev = dev->next) {
      //if (dev->descriptor.idVendor == vendorid && dev->descriptor.idProduct == productid) {  
      if (dev->descriptor.idVendor == vendorid) {  
    device_handle = usb_open(dev);
    //
    // No device found
    if(!device_handle) {
      return NULL;
    }
    //fprintf(stderr," + Found audio transmiter on bus %s dev %s\n", dev->bus->dirname, dev->filename);
    return (device_handle);    
      }
    }
  }
  errno = ENOENT;       // No such device
  return (device_handle);  
}

int help(char *prog) {
   printf("Usage: %s tx pre chan freq pa enable mute\n", prog);
   printf(" The arguments correspond to\n  - TX (0-23)\n  - preemphasis (50us or 75us)\n  - channels (mono or stereo)\n  - frequency (floats from 76.0-108.0)\n  - PA (30 to 120)\n  - enable or disable\n  - mute or unmute\n If one of the arguments is - the default value is set.\n");
   printf("\nExample: %s 0 50us stereo 90.0f 120 enable unmute\n", prog);

   return 1;
}


int keene_sendget(usb_dev_handle *handle, char *senddata ) {
    unsigned char buf[64];
    int rc;

    memcpy(buf, senddata, 0x0000008);
    rc = usb_control_msg(handle, USB_TYPE_CLASS + USB_RECIP_INTERFACE, 0x0000009, 0x0000200, 0x0000002, buf, 0x0000008, 1000);
    //printf("30 control msg returned %d", rc);
    //printf("\n");

    if(rc < 0) {
            perror(" ! Transfer error");
        cleanup();
    }
}

// ** main() ********************************************************
int main(int argc, char *argv[]) {
    int rc;
    int freq_2;
    int freq_3;
    int freq_in;

    if (argc != 8) {
        return help(argv[0]);
    }

    // most of the following code was taken from
    // http://blog.mister-muffin.de/2011/03/14/keene-fm-transmitter/
    // and thus is Copyright (C) 2010-2011 Johannes 'josch' Schauer <j.schauer at email.de>
    // and probably the GPLv3 applies now
    enum settings {
        PREEM_50 = 0x00,
        PREEM_75 = 0x04,
        STEREO = 0x00,
        MONO = 0x01,
        ENABLE = 0x01,
        DISABLE = 0x2,
        MUTE = 0x04,
        UNMUTE = 0x08,
        FREQ = 0x10
    };

    unsigned char conf1[8] = "\x00\x50\x00\x00\x00\x00\x00\x00";
    unsigned char conf2[8] = "\x00\x51\x00\x00\x00\x00\x00\x00";

    int ival, ret;
    float fval;

    // get TX
    if (!strcasecmp(argv[1], "-")) {
        conf2[2] = '\x00';
    } else {
        ret = sscanf(argv[1], "%d", &ival);
        if (ret == 1) {
            if (ival >= 0 && ival <= 23) {
                conf2[2] = (ival%6)<<4 | (23-ival)/6;
            } else {
                fprintf(stderr, "TX must be from 0 to 23\n");
                return 1;
            }
        } else {
            fprintf(stderr, "TX must be integer\n");
            return 1;
        }
    }

    // get preemphasis
    if (!strcasecmp(argv[2], "-") || !strcasecmp(argv[2], "50us")) {
        conf2[3] |= PREEM_50;
    } else if (!strcasecmp(argv[2], "75us")) {
        conf2[3] |= PREEM_75;
    } else {
        fprintf(stderr, "preemphasis must be either 50us or 75us\n");
        return 1;
    }

    // get channels
    if (!strcasecmp(argv[3], "-") || !strcasecmp(argv[3], "stereo")) {
        conf2[3] |= STEREO;
    } else if (!strcasecmp(argv[3], "mono")) {
        conf2[3] |= MONO;
    } else {
        fprintf(stderr, "channels must be mono or stereo\n");
        return 1;
    }

    // get frequency
    if (!strcasecmp(argv[4], "-")) {
        fval = 90.0f;
    } else {
        ret = sscanf(argv[4], "%f", &fval);
        if (ret == 1) {
            if (fval < 76.0f || fval > 108.0f) {
                fprintf(stderr, "frequency must be from 76.0 to 108.0\n");
                return 1;
            }
        } else {
            fprintf(stderr, "frequency must be float\n");
            return 1;
        }
    }
    long int ifreq = lround(20.0f*(fval-76.0f));
    conf1[2] = (ifreq >> 8) & 0xff;
    conf1[3] = ifreq & 0xff;

    // get PA
    if (!strcasecmp(argv[5], "-")) {
        conf1[4] = 120;
    } else {
        ret = sscanf(argv[5], "%d", &ival);
        if (ret == 1) {
            if (ival >= 30 && ival <= 120) {
                conf1[4] = ival;
            } else {
                fprintf(stderr, "PA ust be from 30 to 120\n");
                return 1;
            }
        } else {
            fprintf(stderr, "PA must be integer\n");
            return 1;
        }
    }

    // get enable
    if (!strcasecmp(argv[6], "-") || !strcasecmp(argv[6], "enable")) {
        conf1[5] |= ENABLE;
    } else if (!strcasecmp(argv[6], "disable")) {
        conf1[5] |= DISABLE;
    } else {
        fprintf(stderr, "must be enable or disable");
        return 1;
    }

    // get mute
    if (!strcasecmp(argv[7], "-") || !strcasecmp(argv[7], "unmute")) {
        conf1[5] |= UNMUTE;
    } else if (!strcasecmp(argv[7], "mute")) {
        conf1[5] |= MUTE;
    } else {
        fprintf(stderr, "mute must be mute or unmute");
        return 1;
    }
    conf1[5] |= FREQ;

    //fprintf (stderr,"%d freq_3 %x freqhex %s hexdata \n",freq_3,freq_3,hexdata);

    keenehandle = GetFirstDevice(keeneVID,keenePID);

    // Find the USB connect
    if (!keenehandle) {
        perror(" ! Error opening device!");
        exit(errno);
    }

    // See if we can talk to the device.
    rc = usb_claim_interface(keenehandle,2);
    // If not, we might need to wrestle the keene off the HID driver
    if (rc==-16) {
            //need to grab the device the second bit of the HID device
            rc = usb_detach_kernel_driver_np(keenehandle,2);
            if(rc < 0) {
                perror(" ! usbhid wouldn't let go?");
                cleanup();
            }
        // try again
        rc = usb_claim_interface(keenehandle,2);
    }

    // Claim the interface
    rc = usb_claim_interface(keenehandle,2);
    if(rc < 0) {
        perror(" ! Error claiming the interface (claim interface 2)");
        cleanup();
    }

    keene_sendget(keenehandle,conf1);
    keene_sendget(keenehandle,conf2);

    cleanup();
}
