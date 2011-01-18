#include <stdio.h>
#include <string.h>
#include <usb.h>
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
    fprintf(stderr," + Found audio transmiter on bus %s dev %s\n", dev->bus->dirname, dev->filename);
    return (device_handle);    
      }
    }
  }
  errno = ENOENT;       // No such device
  return (device_handle);  
}

int keene_sendget(usb_dev_handle *handle, char *senddata ) {
    unsigned char buf[64];
    int rc;

    memcpy(buf, senddata, 0x0000008);
    rc = usb_control_msg(handle, USB_TYPE_CLASS + USB_RECIP_INTERFACE, 0x0000009, 0x0000200, 0x0000002, buf, 0x0000008, 1000);
    printf("30 control msg returned %d", rc);
    printf("\n");

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
    unsigned char hexdata[64];

    freq_in = atoi(argv[1]); 
    fprintf (stderr,"begin %d freq_in \n",freq_in);
    if ((freq_in < 8880)&&(freq_in > 8749)) {
        freq_2 = 0;
        freq_3 = ((freq_in -7600)/5);
        fprintf (stderr,"in low %d freq_3 %d freq_2 \n",freq_3,freq_2);
        
    } else if ((freq_in > 10155)&&(freq_in < 10801)) {
        freq_2 = 2;
        freq_3 = ((freq_in -10160)/5);
        fprintf (stderr,"in high %d freq_3 %d freq_2 \n",freq_3,freq_2);

    } else if ((freq_in > 8875 )&&(freq_in < 10160)) {
        freq_2 = 1;
        freq_3 = ((freq_in -8880)/5);
        fprintf (stderr,"in middle %d freq_3 %d freq_2 \n",freq_3,freq_2);
    } else {
        perror(" ! Frequency out of range");
        cleanup;
    }
	
    hexdata[0]=0x00;
    hexdata[1]=0x50;
    hexdata[2]=freq_2;
    hexdata[3]=freq_3;
    hexdata[4]=0x78;
    hexdata[5]=0x19;
    hexdata[6]=0x00;
    hexdata[7]=0x44;

    fprintf (stderr,"%d freq_3 %x freqhex %s hexdata \n",freq_3,freq_3,hexdata);

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
    fprintf(stderr," + Done\n");
    //right we have the device do some stuff to it
    //keep it simple.. set stereo, set frequency, set gain, set on

    //freq data = "\x00\x50\x01\x1b\x78\x19\x00\x34"
    //the fourth character 0x00=88.75
    // 0x40=92.00, one increment =0.05`
    // 0xe0=100.00
    // 0xff=101.55
    keene_sendget(keenehandle,hexdata);

    //Device setup this line should set the TX gain fully on
    //Should set to Europe de-em, Stereo, 50khz, and Europe Band
    keene_sendget(keenehandle, "\x00\x51\x50\x20\x00\x00\x00\x44") ;

    cleanup();
}
