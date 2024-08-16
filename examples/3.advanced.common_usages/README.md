# C++ Sample：3.advanced.common_usages

## Overview

Use the SDK interface to view camera related information, set related parameters, and display the video stream

### Knowledge

Context is the environment context, the first object created during initialization, which can be used to perform some settings, including but not limited to device status change callbacks, log level settings, etc. Context can access multiple Devices.

Device is the device object, which can be used to obtain the device information, such as the model, serial number, and various sensors.One actual hardware device corresponds to one Device object.

## code overview

1. Set the window size and log level, and instantiate a Context class to manage the device

    ```cpp
        // create window for render
        win = std::make_shared<ob_smpl::CVWindow>("CommonUsages", 1280, 720, ob_smpl::ARRANGE_GRID);

        // Set log severity. disable log, please set OB_LOG_SEVERITY_OFF.
        ob::Context::setLoggerSeverity(OB_LOG_SEVERITY_ERROR);

        // Create ob:Context.
        ctx = std::make_shared<ob::Context>();

        // Register device callback
    ```

2. Register device callback

    ```cpp
        ctx.setDeviceChangedCallback( []( std::shared_ptr< ob::DeviceList > removedList, std::shared_ptr< ob::DeviceList > addedList ) {
                DeviceDisconnectCallback( removedList );
                DeviceConnectCallback( addedList );
            } );
    ```

3. Get the device list and print out the information, then use pipeline to start the video stream.

    ```cpp
        // Query the list of connected devices.
        std::shared_ptr<ob::DeviceList> devices = ctx->queryDeviceList();

        // Handle connected devices（and open one device）
        handleDeviceConnected(devices);
    ```

4. Block thread waiting for device connection

    ```cpp
        while(!device) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    ```

5. Execute corresponding settings according to the commands entered by the user. The following is an introduction to some setting functions

    ```cpp
        //Get the basic parameters of the camera, including connection type, device model, etc.
        void getDeviceInformation()
        //Get camera sensor intrinsics, distortion and pixels
        void getCameraParams()
        //Laser switch function
        void switchLaser()
        //Laser safety protection and ranging function switch
        void switchLDP() 
        //Get the laser safety protection and ranging function status
        void getLDPStatus() 
        //Color auto-exposure switch
        void switchColorAE()
        //Color exposure value adjustment
        void setColorExposureValue(bool increase)
        //Color gain value adjustment
        void setColorGainValue(bool increase)
        //Depth auto-exposure switch
        void setDepthExposureValue(bool increase) 
        //Depth exposure value adjustment
        void setDepthGainValue(bool increase)
    ‵‵‵

## Run Sample

Press the button according to the interface prompts

### Result

![image](/docs/resource/common1.png)

![image](/docs/resource/common2.png)