#include "ClientTestFixture.h"

class StreamApiFunctionalityTest : public ClientTestBase {
};

TEST_F(StreamApiFunctionalityTest, putFrame_DescribeStreamDeleted)
{
    CreateStream();
    // Ensure the describe called
    EXPECT_EQ(0, STRCMP(TEST_STREAM_NAME, mStreamName));

    // Move to ready state
    mStreamDescription.version = STREAM_DESCRIPTION_CURRENT_VERSION;
    STRCPY(mStreamDescription.deviceName, TEST_DEVICE_NAME);
    STRCPY(mStreamDescription.streamName, TEST_STREAM_NAME);
    STRCPY(mStreamDescription.contentType, TEST_CONTENT_TYPE);
    STRCPY(mStreamDescription.streamArn, TEST_STREAM_ARN);
    STRCPY(mStreamDescription.updateVersion, TEST_UPDATE_VERSION);
    mStreamDescription.streamStatus = STREAM_STATUS_DELETING;
    mStreamDescription.creationTime = GETTIME();
    // Reset the stream name
    mStreamName[0] = '\0';
    EXPECT_NE(STATUS_SUCCESS, describeStreamResultEvent(mCallContext.customData, SERVICE_CALL_RESULT_OK, &mStreamDescription));
}

TEST_F(StreamApiFunctionalityTest, putFrame_DescribeStreamNotExisting)
{
    CHAR testArn[MAX_ARN_LEN + 1];
    MEMSET(testArn, 'A', MAX_ARN_LEN);

    CreateStream();
    // Ensure the describe called
    EXPECT_EQ(0, STRCMP(TEST_STREAM_NAME, mStreamName));

    // Reset the stream name
    EXPECT_EQ(0, mCreateStreamFuncCount);
    EXPECT_EQ(STATUS_SUCCESS, describeStreamResultEvent(mCallContext.customData, SERVICE_CALL_RESOURCE_NOT_FOUND, NULL));

    EXPECT_EQ(1, mCreateStreamFuncCount);

    // Move the next state
    // Reset the stream name
    mStreamName[0] = '\0';
    // Call with null
    EXPECT_NE(STATUS_SUCCESS, createStreamResultEvent(mCallContext.customData, SERVICE_CALL_RESULT_OK, NULL));
    // Call with larger value
    EXPECT_NE(STATUS_SUCCESS, createStreamResultEvent(mCallContext.customData, SERVICE_CALL_RESULT_OK, testArn));
    // Successful
    EXPECT_EQ(STATUS_SUCCESS, createStreamResultEvent(mCallContext.customData, SERVICE_CALL_RESULT_OK, TEST_STREAM_ARN));

    // Move the next state
    // Reset the stream name
    mStreamName[0] = '\0';
    EXPECT_EQ(STATUS_SUCCESS, getStreamingEndpointResultEvent(mCallContext.customData, SERVICE_CALL_RESULT_OK, TEST_STREAMING_ENDPOINT));

    // Ensure the get token is called
    EXPECT_EQ(0, STRCMP(TEST_STREAM_NAME, mStreamName));
    EXPECT_EQ(STREAM_ACCESS_MODE_READ, mAccessMode);

    // Move to the next state
    // Reset the stream name
    mStreamName[0] = '\0';
    EXPECT_EQ(STATUS_SUCCESS, getStreamingTokenResultEvent(mCallContext.customData, SERVICE_CALL_RESULT_OK, (PBYTE) TEST_STREAMING_TOKEN, SIZEOF(TEST_STREAMING_TOKEN), TEST_AUTH_EXPIRATION));

    // Ensure the callbacks have been called
    EXPECT_EQ(1, mDescribeStreamFuncCount);
    EXPECT_EQ(1, mGetStreamingTokenFuncCount);
    EXPECT_EQ(1, mGetStreamingEndpointFuncCount);
}

TEST_F(StreamApiFunctionalityTest, streamFormatChange_stateCheck)
{
    UINT32 i;
    BYTE tempBuffer[10000];
    UINT64 timestamp;
    Frame frame;
    BYTE cpd[100];

    CreateStream();

    // Ensure we can successfully set the CPD
    EXPECT_EQ(STATUS_SUCCESS, kinesisVideoStreamFormatChanged(mStreamHandle, SIZEOF(cpd), cpd));

    // Move to ready state
    mStreamDescription.version = STREAM_DESCRIPTION_CURRENT_VERSION;
    STRCPY(mStreamDescription.deviceName, TEST_DEVICE_NAME);
    STRCPY(mStreamDescription.streamName, TEST_STREAM_NAME);
    STRCPY(mStreamDescription.contentType, TEST_CONTENT_TYPE);
    STRCPY(mStreamDescription.streamArn, TEST_STREAM_ARN);
    STRCPY(mStreamDescription.updateVersion, TEST_UPDATE_VERSION);
    mStreamDescription.streamStatus = STREAM_STATUS_ACTIVE;
    mStreamDescription.creationTime = GETTIME();
    EXPECT_EQ(STATUS_SUCCESS, describeStreamResultEvent(mCallContext.customData, SERVICE_CALL_RESULT_OK, &mStreamDescription));

    // Ensure we can successfully set the CPD
    EXPECT_EQ(STATUS_SUCCESS, kinesisVideoStreamFormatChanged(mStreamHandle, SIZEOF(cpd), cpd));

    EXPECT_EQ(STATUS_SUCCESS, getStreamingEndpointResultEvent(mCallContext.customData, SERVICE_CALL_RESULT_OK, TEST_STREAMING_ENDPOINT));

    // Ensure we can successfully set the CPD
    EXPECT_EQ(STATUS_SUCCESS, kinesisVideoStreamFormatChanged(mStreamHandle, SIZEOF(cpd), cpd));

    EXPECT_EQ(STATUS_SUCCESS, getStreamingTokenResultEvent(mCallContext.customData, SERVICE_CALL_RESULT_OK, (PBYTE) TEST_STREAMING_TOKEN, SIZEOF(TEST_STREAMING_TOKEN), TEST_AUTH_EXPIRATION));

    // Ensure we can successfully set the CPD
    EXPECT_EQ(STATUS_SUCCESS, kinesisVideoStreamFormatChanged(mStreamHandle, SIZEOF(cpd), cpd));

    for (i = 0, timestamp = 0; i < 20; timestamp += TEST_FRAME_DURATION, i++) {
        frame.index = i;
        frame.decodingTs = timestamp;
        frame.presentationTs = timestamp;
        frame.duration = TEST_FRAME_DURATION;
        frame.size = SIZEOF(tempBuffer);
        frame.frameData = tempBuffer;

        // Key frame every 4th
        frame.flags = i % 4 == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;
        EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

        // Return a put stream result on 5th
        if (i == 5) {
            EXPECT_EQ(STATUS_SUCCESS, putStreamResultEvent(mCallContext.customData, SERVICE_CALL_RESULT_OK, TEST_STREAMING_HANDLE));
        }

        // Setting CPD should fail
        EXPECT_NE(STATUS_SUCCESS, kinesisVideoStreamFormatChanged(mStreamHandle, SIZEOF(cpd), cpd));
    }
}

TEST_F(StreamApiFunctionalityTest, putFrame_BasicPutTestItemLimit)
{
    UINT32 i, maxIteration;
    BYTE tempBuffer[10000];
    UINT64 timestamp;
    Frame frame;

    // Create and ready a stream
    ReadyStream();

    // We should have space for 40 seconds
    maxIteration = 24 * ((UINT32)(TEST_BUFFER_DURATION / HUNDREDS_OF_NANOS_IN_A_SECOND));

    // Iterate 2 items over the buffer limit
    for (i = 0, timestamp = 0; i < maxIteration + 2; timestamp += TEST_FRAME_DURATION, i++) {
        frame.index = i;
        frame.decodingTs = timestamp;
        frame.presentationTs = timestamp;
        frame.duration = TEST_FRAME_DURATION;
        frame.size = SIZEOF(tempBuffer);
        frame.frameData = tempBuffer;

        // Key frame every 10th
        frame.flags = i % 10 == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;
        EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

        // Ensure put stream is called
        EXPECT_EQ(0, STRCMP(TEST_STREAM_NAME, mStreamName));
        EXPECT_EQ(TRUE, mAckRequired);
        // Need to ensure we have a streaming token in the auth
        EXPECT_EQ(SIZEOF(TEST_STREAMING_TOKEN), mCallContext.pAuthInfo->size);
        EXPECT_EQ(0, STRCMP(TEST_STREAMING_TOKEN, (PCHAR) mCallContext.pAuthInfo->data));

        if (i == maxIteration) {
            // Should have dropped but the first frame with ts = 0
            EXPECT_EQ(0, mFrameTime);
        } else if (i == maxIteration + 1) {
            EXPECT_NE(0, mFrameTime);
        } else {
            // Drop frame hasn't been called
            EXPECT_EQ(0, mFrameTime);
        }

        // Return a put stream result on 50th
        if (i == 50) {
            EXPECT_EQ(STATUS_SUCCESS, putStreamResultEvent(mCallContext.customData, SERVICE_CALL_RESULT_OK, TEST_STREAMING_HANDLE));
        }
    }
}

TEST_F(StreamApiFunctionalityTest, putFrame_BasicPutTestDurationLimit)
{
    UINT32 i;
    BYTE tempBuffer[10000];
    UINT64 timestamp;
    Frame frame;

    // Create and ready a stream
    ReadyStream();

    // Iterate 2 items over the buffer limit
    for (i = 0, timestamp = 0; timestamp < TEST_BUFFER_DURATION + 2 * TEST_LONG_FRAME_DURATION; timestamp += TEST_LONG_FRAME_DURATION, i++) {
        frame.index = i;
        frame.decodingTs = timestamp;
        frame.presentationTs = timestamp;
        frame.duration = TEST_LONG_FRAME_DURATION;
        frame.size = SIZEOF(tempBuffer);
        frame.frameData = tempBuffer;

        // Key frame every 10th
        frame.flags = i % 10 == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;
        EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

        // Ensure put stream is called
        EXPECT_EQ(0, STRCMP(TEST_STREAM_NAME, mStreamName));
        EXPECT_EQ(TRUE, mAckRequired);
        // Need to ensure we have a streaming token in the auth
        EXPECT_EQ(SIZEOF(TEST_STREAMING_TOKEN), mCallContext.pAuthInfo->size);
        EXPECT_EQ(0, STRCMP(TEST_STREAMING_TOKEN, (PCHAR) mCallContext.pAuthInfo->data));

        if (timestamp == TEST_BUFFER_DURATION) {
            // Should have dropped but the first frame with ts = 0
            EXPECT_EQ(0, mFrameTime);
        } else if (timestamp == TEST_BUFFER_DURATION + TEST_LONG_FRAME_DURATION) {
            EXPECT_NE(0, mFrameTime);
        } else {
            // Drop frame hasn't been called
            EXPECT_EQ(0, mFrameTime);
        }

        // Return a put stream result on 50th
        if (i == 50) {
            EXPECT_EQ(STATUS_SUCCESS, putStreamResultEvent(mCallContext.customData, SERVICE_CALL_RESULT_OK, TEST_STREAMING_HANDLE));
        }
    }
}

TEST_F(StreamApiFunctionalityTest, putFrame_PutGetUnderflow)
{
    UINT32 i, filledSize;
    BYTE tempBuffer[10000];
    BYTE getDataBuffer[20000];
    UINT64 timestamp, clientStreamHandle;
    Frame frame;

    // Create and ready a stream
    ReadyStream();

    for (i = 0, timestamp = 0; timestamp < TEST_BUFFER_DURATION; timestamp += TEST_LONG_FRAME_DURATION, i++) {
        frame.index = i;
        frame.decodingTs = timestamp;
        frame.presentationTs = timestamp;
        frame.duration = TEST_LONG_FRAME_DURATION;
        frame.size = SIZEOF(tempBuffer);
        frame.frameData = tempBuffer;

        // Key frame every 10th
        frame.flags = i % 10 == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;
        EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

        // Ensure put stream is called
        EXPECT_EQ(0, STRCMP(TEST_STREAM_NAME, mStreamName));
        EXPECT_EQ(TRUE, mAckRequired);
        // Need to ensure we have a streaming token in the auth
        EXPECT_EQ(SIZEOF(TEST_STREAMING_TOKEN), mCallContext.pAuthInfo->size);
        EXPECT_EQ(0, STRCMP(TEST_STREAMING_TOKEN, (PCHAR) mCallContext.pAuthInfo->data));

        // Drop frame hasn't been called
        EXPECT_EQ(0, mFrameTime);

        // Return a put stream result on 50th
        if (i == 50) {
            EXPECT_EQ(STATUS_SUCCESS, putStreamResultEvent(mCallContext.customData, SERVICE_CALL_RESULT_OK, TEST_STREAMING_HANDLE));
        }
    }

    // Now try to retrieve the data
    for (timestamp = 0; timestamp < TEST_BUFFER_DURATION / 2; timestamp += TEST_LONG_FRAME_DURATION) {
        clientStreamHandle = 0;
        EXPECT_EQ(STATUS_SUCCESS,
                  getKinesisVideoStreamData(mStreamHandle, &clientStreamHandle, getDataBuffer, SIZEOF(getDataBuffer),
                                            &filledSize));
        EXPECT_EQ(SIZEOF(getDataBuffer), filledSize);
        EXPECT_EQ(TEST_STREAMING_HANDLE, clientStreamHandle);
    }

    clientStreamHandle = 0;
    EXPECT_TRUE(STATUS_FAILED(
            getKinesisVideoStreamData(mStreamHandle, &clientStreamHandle, getDataBuffer, SIZEOF(getDataBuffer),
                                      &filledSize)));
    EXPECT_EQ(TEST_STREAMING_HANDLE, clientStreamHandle);

    clientStreamHandle = 0;
    EXPECT_TRUE(STATUS_FAILED(
            getKinesisVideoStreamData(mStreamHandle, &clientStreamHandle, getDataBuffer, SIZEOF(getDataBuffer),
                                      &filledSize)));
    EXPECT_EQ(TEST_STREAMING_HANDLE, clientStreamHandle);
}

TEST_F(StreamApiFunctionalityTest, putFrame_PutGetNextKeyFrame)
{
    UINT32 i, filledSize;
    BYTE tempBuffer[10000];
    PBYTE pData;
    BYTE getDataBuffer[20000];
    UINT64 timestamp, clientStreamHandle;
    Frame frame;

    // Create and ready a stream
    ReadyStream();

    // Make sure we drop the first frame which should be the key frame
    for (i = 0, timestamp = 0; timestamp < TEST_BUFFER_DURATION + TEST_LONG_FRAME_DURATION; timestamp += TEST_LONG_FRAME_DURATION, i++) {
        frame.index = i;
        frame.decodingTs = timestamp;
        frame.presentationTs = timestamp;
        frame.duration = TEST_LONG_FRAME_DURATION;
        frame.size = SIZEOF(tempBuffer);
        // Change the content of the buffer
        *(PUINT32) &tempBuffer = i;
        frame.frameData = tempBuffer;

        // Key frame every 10th
        frame.flags = i % 10 == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;
        EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

        // Ensure put stream is called
        EXPECT_EQ(0, STRCMP(TEST_STREAM_NAME, mStreamName));
        EXPECT_EQ(TRUE, mAckRequired);
        // Need to ensure we have a streaming token in the auth
        EXPECT_EQ(SIZEOF(TEST_STREAMING_TOKEN), mCallContext.pAuthInfo->size);
        EXPECT_EQ(0, STRCMP(TEST_STREAMING_TOKEN, (PCHAR) mCallContext.pAuthInfo->data));

        EXPECT_EQ(0, mFrameTime);

        // Return a put stream result on 50th
        if (i == 50) {
            EXPECT_EQ(STATUS_SUCCESS, putStreamResultEvent(mCallContext.customData, SERVICE_CALL_RESULT_OK, TEST_STREAMING_HANDLE));
        }
    }

    // Now, the first frame should be the 10th
    clientStreamHandle = 0;
    EXPECT_EQ(STATUS_SUCCESS,
              getKinesisVideoStreamData(mStreamHandle, &clientStreamHandle, getDataBuffer, SIZEOF(getDataBuffer),
                                        &filledSize));
    EXPECT_EQ(SIZEOF(getDataBuffer), filledSize);
    EXPECT_EQ(TEST_STREAMING_HANDLE, clientStreamHandle);
    pData = getDataBuffer;

    // This should point to 10th frame, offset with cluster info and simple block info
    EXPECT_EQ(10, *(PUINT32) (pData + MKV_CLUSTER_INFO_BITS_SIZE + MKV_SIMPLE_BLOCK_BITS_SIZE));

    // Set a new value to ensure it doesn't change as there is no dropped frames when we put again
    mFrameTime = 1000000;

    timestamp += TEST_LONG_FRAME_DURATION;
    frame.index = i + 1;
    frame.decodingTs = timestamp;
    frame.presentationTs = timestamp;
    frame.duration = TEST_LONG_FRAME_DURATION;
    frame.size = SIZEOF(tempBuffer);
    // Change the content of the buffer
    *(PUINT32) &tempBuffer = i;
    frame.frameData = tempBuffer;

    EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));
    EXPECT_EQ(1000000, mFrameTime);
}

TEST_F(StreamApiFunctionalityTest, putFrame_StorageOverflow)
{
    UINT32 i;
    UINT32 frameSize = 1000000;
    PBYTE pData = (PBYTE) MEMALLOC(frameSize);
    UINT64 timestamp, clientStreamHandle;
    Frame frame;

    // Create and ready a stream
    ReadyStream();

    // Make sure we drop the first frame which should be the key frame
    for (i = 0, timestamp = 0; i < TEST_DEVICE_STORAGE_SIZE / frameSize; timestamp += TEST_LONG_FRAME_DURATION, i++) {
        frame.index = i;
        frame.decodingTs = timestamp;
        frame.presentationTs = timestamp;
        frame.duration = TEST_LONG_FRAME_DURATION;
        frame.size = frameSize;
        // Change the content of the buffer
        *(PUINT32) pData = i;
        frame.frameData = pData;

        // Key frame every 3rd
        frame.flags = i % 3 == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;
        EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

        // Ensure put stream is called
        EXPECT_EQ(0, STRCMP(TEST_STREAM_NAME, mStreamName));
        EXPECT_EQ(TRUE, mAckRequired);
        // Need to ensure we have a streaming token in the auth
        EXPECT_EQ(SIZEOF(TEST_STREAMING_TOKEN), mCallContext.pAuthInfo->size);
        EXPECT_EQ(0, STRCMP(TEST_STREAMING_TOKEN, (PCHAR) mCallContext.pAuthInfo->data));

        EXPECT_EQ(0, mFrameTime);

        // Return a put stream result on 5th
        if (i == 5) {
            EXPECT_EQ(STATUS_SUCCESS, putStreamResultEvent(mCallContext.customData, SERVICE_CALL_RESULT_OK, TEST_STREAMING_HANDLE));
        }
    }

    // Now, we should overflow
    mFrameTime = 1000000;

    timestamp += TEST_LONG_FRAME_DURATION;
    frame.index = i + 1;
    frame.decodingTs = timestamp;
    frame.presentationTs = timestamp;
    frame.duration = TEST_LONG_FRAME_DURATION;
    frame.size = frameSize;
    // Change the content of the buffer
    *(PUINT32) pData = i;
    frame.frameData = pData;

    EXPECT_EQ(STATUS_STORE_OUT_OF_MEMORY, putKinesisVideoFrame(mStreamHandle, &frame));
    EXPECT_EQ(1000000, mFrameTime);

    MEMFREE(pData);
}

TEST_F(StreamApiFunctionalityTest, putFrame_StoragePressureNotification)
{
    UINT32 i, remaining;
    UINT32 frameSize = 100000;
    PBYTE pData = (PBYTE) MEMALLOC(frameSize);
    UINT64 timestamp, clientStreamHandle;
    Frame frame;

    // Create and ready a stream
    ReadyStream();

    // Reset the indicator of the callback called
    mRemainingSize = 0;
    STORAGE_PRESSURE_NOTIFICATION_THRESHOLD;

    // Make sure we drop the first frame which should be the key frame
    for (i = 0, timestamp = 0; i < TEST_DEVICE_STORAGE_SIZE / frameSize; timestamp += TEST_LONG_FRAME_DURATION, i++) {
        frame.index = i;
        frame.decodingTs = timestamp;
        frame.presentationTs = timestamp;
        frame.duration = TEST_LONG_FRAME_DURATION;
        frame.size = frameSize;
        // Change the content of the buffer
        *(PUINT32) pData = i;
        frame.frameData = pData;

        // Key frame every 3rd
        frame.flags = i % 3 == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;
        EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

        // Ensure put stream is called
        EXPECT_EQ(0, STRCMP(TEST_STREAM_NAME, mStreamName));
        EXPECT_EQ(TRUE, mAckRequired);
        // Need to ensure we have a streaming token in the auth
        EXPECT_EQ(SIZEOF(TEST_STREAMING_TOKEN), mCallContext.pAuthInfo->size);
        EXPECT_EQ(0, STRCMP(TEST_STREAMING_TOKEN, (PCHAR) mCallContext.pAuthInfo->data));

        // Ensure the pressure notification is called when we are below STORAGE_PRESSURE_NOTIFICATION_THRESHOLD
        remaining = TEST_DEVICE_STORAGE_SIZE - (frameSize * i);
        if ((UINT32) ((DOUBLE) remaining / TEST_DEVICE_STORAGE_SIZE * 100) <= STORAGE_PRESSURE_NOTIFICATION_THRESHOLD) {
            EXPECT_NE(0, mRemainingSize);
        }

        // Return a put stream result on 5th
        if (i == 5) {
            EXPECT_EQ(STATUS_SUCCESS, putStreamResultEvent(mCallContext.customData, SERVICE_CALL_RESULT_OK, TEST_STREAMING_HANDLE));
        }
    }

    MEMFREE(pData);
}

TEST_F(StreamApiFunctionalityTest, putFrame_StreamLatencyNotification)
{
    UINT32 i;
    BYTE tempBuffer[10000];
    UINT64 timestamp, latency;
    Frame frame;

    // Set the latency pressure threshold
    latency = TEST_LONG_FRAME_DURATION * 30;
    mStreamInfo.streamCaps.maxLatency = latency;

    // Create and ready a stream
    ReadyStream();

    for (i = 0, timestamp = 0; timestamp <= TEST_BUFFER_DURATION; timestamp += TEST_LONG_FRAME_DURATION, i++) {
        frame.index = i;
        frame.decodingTs = timestamp;
        frame.presentationTs = timestamp;
        frame.duration = TEST_LONG_FRAME_DURATION;
        frame.size = SIZEOF(tempBuffer);
        frame.frameData = tempBuffer;

        // Key frame every 10th
        frame.flags = i % 10 == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;
        EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

        if (timestamp == latency) {
            // Should have called a pressure notification
            EXPECT_EQ(1, mStreamLatencyPressureFuncCount);
            EXPECT_EQ(latency + TEST_LONG_FRAME_DURATION, mDuration);
        } else if (timestamp == TEST_BUFFER_DURATION) {
            EXPECT_EQ(i - latency / TEST_LONG_FRAME_DURATION + 1, mStreamLatencyPressureFuncCount);
            EXPECT_EQ(TEST_BUFFER_DURATION, mDuration);
        } else if (timestamp < latency) {
            // Shouldn't have been called yet
            EXPECT_EQ(0, mStreamLatencyPressureFuncCount);
            EXPECT_EQ(0, mDuration);
        }
    }
}

TEST_F(StreamApiFunctionalityTest, putFrame_PutGetRestartOkResult)
{
    UINT32 i, filledSize;
    BYTE tempBuffer[10000];
    BYTE getDataBuffer[20000];
    UINT64 timestamp, clientStreamHandle;
    Frame frame;

    // Create and ready a stream
    ReadyStream();

    frame.duration = TEST_LONG_FRAME_DURATION;
    frame.size = SIZEOF(tempBuffer);
    frame.frameData = tempBuffer;
    for (i = 0, timestamp = 0; timestamp < TEST_BUFFER_DURATION; timestamp += TEST_LONG_FRAME_DURATION, i++) {
        frame.index = i;
        frame.decodingTs = timestamp;
        frame.presentationTs = timestamp;

        // Key frame every 10th
        frame.flags = i % 10 == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;
        EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

        // Ensure put stream is called
        EXPECT_EQ(1, mPutStreamFuncCount);
        EXPECT_EQ(0, STRCMP(TEST_STREAM_NAME, mStreamName));
        EXPECT_EQ(0, STRCMP(TEST_STREAMING_ENDPOINT, mStreamingEndpoint));
        EXPECT_EQ(TRUE, mAckRequired);
        // Need to ensure we have a streaming token in the auth
        EXPECT_EQ(SIZEOF(TEST_STREAMING_TOKEN), mCallContext.pAuthInfo->size);
        EXPECT_EQ(0, STRCMP(TEST_STREAMING_TOKEN, (PCHAR) mCallContext.pAuthInfo->data));

        // Drop frame hasn't been called
        EXPECT_EQ(0, mFrameTime);

        // Return a put stream result on 50th
        if (i == 50) {
            EXPECT_EQ(STATUS_SUCCESS, putStreamResultEvent(mCallContext.customData, SERVICE_CALL_RESULT_OK, TEST_STREAMING_HANDLE));
        }
    }

    // Now try to retrieve the data
    for (timestamp = 0; timestamp < TEST_BUFFER_DURATION / 2; timestamp += TEST_LONG_FRAME_DURATION) {
        clientStreamHandle = 0;
        EXPECT_EQ(STATUS_SUCCESS,
                  getKinesisVideoStreamData(mStreamHandle, &clientStreamHandle, getDataBuffer, SIZEOF(getDataBuffer),
                                            &filledSize));
        EXPECT_EQ(SIZEOF(getDataBuffer), filledSize);
        EXPECT_EQ(TEST_STREAMING_HANDLE, clientStreamHandle);
    }

    // Send a terminate event to warm reset the stream
    EXPECT_EQ(STATUS_SUCCESS, kinesisVideoStreamTerminated(mCallContext.customData, TEST_STREAMING_HANDLE, SERVICE_CALL_RESULT_OK));

    // Try streaming again and ensure the right callbacks are fired
    frame.index++;
    frame.decodingTs += TEST_LONG_FRAME_DURATION;
    frame.presentationTs += TEST_LONG_FRAME_DURATION;
    frame.flags = FRAME_FLAG_KEY_FRAME;
    EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

    // After restarting we need to satisfy the callbacks
    MoveFromEndpointToReady();

    EXPECT_EQ(2, mPutStreamFuncCount);
    EXPECT_EQ(2, mGetStreamingEndpointFuncCount);
    EXPECT_EQ(2, mGetStreamingTokenFuncCount);

    // Put frame again to ensure there are no more callbacks
    frame.index++;
    frame.decodingTs += TEST_LONG_FRAME_DURATION;
    frame.presentationTs += TEST_LONG_FRAME_DURATION;
    frame.flags = FRAME_FLAG_KEY_FRAME;
    EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

    EXPECT_EQ(2, mPutStreamFuncCount);
    EXPECT_EQ(2, mGetStreamingEndpointFuncCount);
    EXPECT_EQ(2, mGetStreamingTokenFuncCount);
}

TEST_F(StreamApiFunctionalityTest, putFrame_PutGetRestartStreamLimitResult)
{
    UINT32 i, filledSize;
    BYTE tempBuffer[10000];
    BYTE getDataBuffer[20000];
    UINT64 timestamp, clientStreamHandle;
    Frame frame;

    // Create and ready a stream
    ReadyStream();

    frame.duration = TEST_LONG_FRAME_DURATION;
    frame.size = SIZEOF(tempBuffer);
    frame.frameData = tempBuffer;
    for (i = 0, timestamp = 0; timestamp < TEST_BUFFER_DURATION; timestamp += TEST_LONG_FRAME_DURATION, i++) {
        frame.index = i;
        frame.decodingTs = timestamp;
        frame.presentationTs = timestamp;

        // Key frame every 10th
        frame.flags = i % 10 == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;
        EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

        // Ensure put stream is called
        EXPECT_EQ(1, mPutStreamFuncCount);
        EXPECT_EQ(0, STRCMP(TEST_STREAM_NAME, mStreamName));
        EXPECT_EQ(0, STRCMP(TEST_STREAMING_ENDPOINT, mStreamingEndpoint));
        EXPECT_EQ(TRUE, mAckRequired);
        // Need to ensure we have a streaming token in the auth
        EXPECT_EQ(SIZEOF(TEST_STREAMING_TOKEN), mCallContext.pAuthInfo->size);
        EXPECT_EQ(0, STRCMP(TEST_STREAMING_TOKEN, (PCHAR) mCallContext.pAuthInfo->data));

        // Drop frame hasn't been called
        EXPECT_EQ(0, mFrameTime);

        // Return a put stream result on 50th
        if (i == 50) {
            EXPECT_EQ(STATUS_SUCCESS, putStreamResultEvent(mCallContext.customData, SERVICE_CALL_RESULT_OK, TEST_STREAMING_HANDLE));
        }
    }

    // Now try to retrieve the data
    for (timestamp = 0; timestamp < TEST_BUFFER_DURATION / 2; timestamp += TEST_LONG_FRAME_DURATION) {
        clientStreamHandle = 0;
        EXPECT_EQ(STATUS_SUCCESS,
                  getKinesisVideoStreamData(mStreamHandle, &clientStreamHandle, getDataBuffer, SIZEOF(getDataBuffer),
                                            &filledSize));
        EXPECT_EQ(SIZEOF(getDataBuffer), filledSize);
        EXPECT_EQ(TEST_STREAMING_HANDLE, clientStreamHandle);
    }

    // Send a terminate event to warm reset the stream
    EXPECT_EQ(STATUS_SUCCESS, kinesisVideoStreamTerminated(mCallContext.customData, TEST_STREAMING_HANDLE, SERVICE_CALL_STREAM_LIMIT));

    // Try streaming again and ensure the right callbacks are fired
    frame.index++;
    frame.decodingTs += TEST_LONG_FRAME_DURATION;
    frame.presentationTs += TEST_LONG_FRAME_DURATION;
    frame.flags = FRAME_FLAG_KEY_FRAME;
    EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

    // After restarting we need to satisfy the callbacks
    MoveFromEndpointToReady();

    EXPECT_EQ(2, mPutStreamFuncCount);
    EXPECT_EQ(2, mGetStreamingEndpointFuncCount);
    EXPECT_EQ(2, mGetStreamingTokenFuncCount);

    // Put frame again to ensure there are no more callbacks
    frame.index++;
    frame.decodingTs += TEST_LONG_FRAME_DURATION;
    frame.presentationTs += TEST_LONG_FRAME_DURATION;
    frame.flags = FRAME_FLAG_KEY_FRAME;
    EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

    EXPECT_EQ(2, mPutStreamFuncCount);
    EXPECT_EQ(2, mGetStreamingEndpointFuncCount);
    EXPECT_EQ(2, mGetStreamingTokenFuncCount);
}

TEST_F(StreamApiFunctionalityTest, putFrame_PutGetRestartUnauthorizedResult)
{
    UINT32 i, filledSize;
    BYTE tempBuffer[10000];
    BYTE getDataBuffer[20000];
    UINT64 timestamp, clientStreamHandle;
    Frame frame;

    // Create and ready a stream
    ReadyStream();

    frame.duration = TEST_LONG_FRAME_DURATION;
    frame.size = SIZEOF(tempBuffer);
    frame.frameData = tempBuffer;
    for (i = 0, timestamp = 0; timestamp < TEST_BUFFER_DURATION; timestamp += TEST_LONG_FRAME_DURATION, i++) {
        frame.index = i;
        frame.decodingTs = timestamp;
        frame.presentationTs = timestamp;

        // Key frame every 10th
        frame.flags = i % 10 == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;
        EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

        // Ensure put stream is called
        EXPECT_EQ(1, mPutStreamFuncCount);
        EXPECT_EQ(0, STRCMP(TEST_STREAM_NAME, mStreamName));
        EXPECT_EQ(0, STRCMP(TEST_STREAMING_ENDPOINT, mStreamingEndpoint));
        EXPECT_EQ(TRUE, mAckRequired);
        // Need to ensure we have a streaming token in the auth
        EXPECT_EQ(SIZEOF(TEST_STREAMING_TOKEN), mCallContext.pAuthInfo->size);
        EXPECT_EQ(0, STRCMP(TEST_STREAMING_TOKEN, (PCHAR) mCallContext.pAuthInfo->data));

        // Drop frame hasn't been called
        EXPECT_EQ(0, mFrameTime);

        // Return a put stream result on 50th
        if (i == 50) {
            EXPECT_EQ(STATUS_SUCCESS, putStreamResultEvent(mCallContext.customData, SERVICE_CALL_RESULT_OK, TEST_STREAMING_HANDLE));
        }
    }

    // Now try to retrieve the data
    for (timestamp = 0; timestamp < TEST_BUFFER_DURATION / 2; timestamp += TEST_LONG_FRAME_DURATION) {
        clientStreamHandle = 0;
        EXPECT_EQ(STATUS_SUCCESS,
                  getKinesisVideoStreamData(mStreamHandle, &clientStreamHandle, getDataBuffer, SIZEOF(getDataBuffer),
                                            &filledSize));
        EXPECT_EQ(SIZEOF(getDataBuffer), filledSize);
        EXPECT_EQ(TEST_STREAMING_HANDLE, clientStreamHandle);
    }

    // Send a terminate event to warm reset the stream
    EXPECT_EQ(STATUS_SUCCESS, kinesisVideoStreamTerminated(mCallContext.customData, TEST_STREAMING_HANDLE, SERVICE_CALL_NOT_AUTHORIZED));

    // Try streaming again and ensure the right callbacks are fired
    frame.index++;
    frame.decodingTs += TEST_LONG_FRAME_DURATION;
    frame.presentationTs += TEST_LONG_FRAME_DURATION;
    frame.flags = FRAME_FLAG_KEY_FRAME;
    EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

    // After restarting we need to satisfy the callbacks
    MoveFromTokenToReady();

    EXPECT_EQ(2, mPutStreamFuncCount);
    EXPECT_EQ(1, mGetStreamingEndpointFuncCount);
    EXPECT_EQ(2, mGetStreamingTokenFuncCount);

    // Put frame again to ensure there are no more callbacks
    frame.index++;
    frame.decodingTs += TEST_LONG_FRAME_DURATION;
    frame.presentationTs += TEST_LONG_FRAME_DURATION;
    frame.flags = FRAME_FLAG_KEY_FRAME;
    EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

    EXPECT_EQ(2, mPutStreamFuncCount);
    EXPECT_EQ(1, mGetStreamingEndpointFuncCount);
    EXPECT_EQ(2, mGetStreamingTokenFuncCount);
}

TEST_F(StreamApiFunctionalityTest, putFrame_PutGetRestartOtherResult)
{
    UINT32 i, filledSize;
    BYTE tempBuffer[10000];
    BYTE getDataBuffer[20000];
    UINT64 timestamp, clientStreamHandle;
    Frame frame;

    // Create and ready a stream
    ReadyStream();

    frame.duration = TEST_LONG_FRAME_DURATION;
    frame.size = SIZEOF(tempBuffer);
    frame.frameData = tempBuffer;
    for (i = 0, timestamp = 0; timestamp < TEST_BUFFER_DURATION; timestamp += TEST_LONG_FRAME_DURATION, i++) {
        frame.index = i;
        frame.decodingTs = timestamp;
        frame.presentationTs = timestamp;

        // Key frame every 10th
        frame.flags = i % 10 == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;
        EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

        // Ensure put stream is called
        EXPECT_EQ(1, mPutStreamFuncCount);
        EXPECT_EQ(0, STRCMP(TEST_STREAM_NAME, mStreamName));
        EXPECT_EQ(0, STRCMP(TEST_STREAMING_ENDPOINT, mStreamingEndpoint));
        EXPECT_EQ(TRUE, mAckRequired);
        // Need to ensure we have a streaming token in the auth
        EXPECT_EQ(SIZEOF(TEST_STREAMING_TOKEN), mCallContext.pAuthInfo->size);
        EXPECT_EQ(0, STRCMP(TEST_STREAMING_TOKEN, (PCHAR) mCallContext.pAuthInfo->data));

        // Drop frame hasn't been called
        EXPECT_EQ(0, mFrameTime);

        // Return a put stream result on 50th
        if (i == 50) {
            EXPECT_EQ(STATUS_SUCCESS, putStreamResultEvent(mCallContext.customData, SERVICE_CALL_RESULT_OK, TEST_STREAMING_HANDLE));
        }
    }

    // Now try to retrieve the data
    for (timestamp = 0; timestamp < TEST_BUFFER_DURATION / 2; timestamp += TEST_LONG_FRAME_DURATION) {
        clientStreamHandle = 0;
        EXPECT_EQ(STATUS_SUCCESS,
                  getKinesisVideoStreamData(mStreamHandle, &clientStreamHandle, getDataBuffer, SIZEOF(getDataBuffer),
                                            &filledSize));
        EXPECT_EQ(SIZEOF(getDataBuffer), filledSize);
        EXPECT_EQ(TEST_STREAMING_HANDLE, clientStreamHandle);
    }

    // Send a terminate event to warm reset the stream
    EXPECT_EQ(STATUS_SUCCESS, kinesisVideoStreamTerminated(mCallContext.customData, TEST_STREAMING_HANDLE, SERVICE_CALL_UNKNOWN));

    // Try streaming again and ensure the right callbacks are fired
    frame.index++;
    frame.decodingTs += TEST_LONG_FRAME_DURATION;
    frame.presentationTs += TEST_LONG_FRAME_DURATION;
    frame.flags = FRAME_FLAG_KEY_FRAME;
    EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

    // After restarting we need to satisfy the callbacks
    MoveFromDescribeToReady();

    EXPECT_EQ(2, mPutStreamFuncCount);
    EXPECT_EQ(2, mGetStreamingEndpointFuncCount);
    EXPECT_EQ(2, mGetStreamingTokenFuncCount);
    EXPECT_EQ(2, mDescribeStreamFuncCount);

    // Put frame again to ensure there are no more callbacks
    frame.index++;
    frame.decodingTs += TEST_LONG_FRAME_DURATION;
    frame.presentationTs += TEST_LONG_FRAME_DURATION;
    frame.flags = FRAME_FLAG_KEY_FRAME;
    EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

    EXPECT_EQ(2, mPutStreamFuncCount);
    EXPECT_EQ(2, mGetStreamingEndpointFuncCount);
    EXPECT_EQ(2, mGetStreamingTokenFuncCount);
    EXPECT_EQ(2, mDescribeStreamFuncCount);
}

TEST_F(StreamApiFunctionalityTest, putFrame_PutGetRestartNotFoundResult)
{
    UINT32 i, filledSize;
    BYTE tempBuffer[10000];
    BYTE getDataBuffer[20000];
    UINT64 timestamp, clientStreamHandle;
    Frame frame;

    // Create and ready a stream
    ReadyStream();

    frame.duration = TEST_LONG_FRAME_DURATION;
    frame.size = SIZEOF(tempBuffer);
    frame.frameData = tempBuffer;
    for (i = 0, timestamp = 0; timestamp < TEST_BUFFER_DURATION; timestamp += TEST_LONG_FRAME_DURATION, i++) {
        frame.index = i;
        frame.decodingTs = timestamp;
        frame.presentationTs = timestamp;

        // Key frame every 10th
        frame.flags = i % 10 == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;
        EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

        // Ensure put stream is called
        EXPECT_EQ(1, mPutStreamFuncCount);
        EXPECT_EQ(0, STRCMP(TEST_STREAM_NAME, mStreamName));
        EXPECT_EQ(0, STRCMP(TEST_STREAMING_ENDPOINT, mStreamingEndpoint));
        EXPECT_EQ(TRUE, mAckRequired);
        // Need to ensure we have a streaming token in the auth
        EXPECT_EQ(SIZEOF(TEST_STREAMING_TOKEN), mCallContext.pAuthInfo->size);
        EXPECT_EQ(0, STRCMP(TEST_STREAMING_TOKEN, (PCHAR) mCallContext.pAuthInfo->data));

        // Drop frame hasn't been called
        EXPECT_EQ(0, mFrameTime);

        // Return a put stream result on 50th
        if (i == 50) {
            EXPECT_EQ(STATUS_SUCCESS, putStreamResultEvent(mCallContext.customData, SERVICE_CALL_RESULT_OK, TEST_STREAMING_HANDLE));
        }
    }

    // Now try to retrieve the data
    for (timestamp = 0; timestamp < TEST_BUFFER_DURATION / 2; timestamp += TEST_LONG_FRAME_DURATION) {
        clientStreamHandle = 0;
        EXPECT_EQ(STATUS_SUCCESS,
                  getKinesisVideoStreamData(mStreamHandle, &clientStreamHandle, getDataBuffer, SIZEOF(getDataBuffer),
                                            &filledSize));
        EXPECT_EQ(SIZEOF(getDataBuffer), filledSize);
        EXPECT_EQ(TEST_STREAMING_HANDLE, clientStreamHandle);
    }

    // Send a terminate event to warm reset the stream
    EXPECT_EQ(STATUS_SUCCESS, kinesisVideoStreamTerminated(mCallContext.customData, TEST_STREAMING_HANDLE, SERVICE_CALL_RESOURCE_NOT_FOUND));

    // Try streaming again and ensure the right callbacks are fired
    frame.index++;
    frame.decodingTs += TEST_LONG_FRAME_DURATION;
    frame.presentationTs += TEST_LONG_FRAME_DURATION;
    frame.flags = FRAME_FLAG_KEY_FRAME;
    EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

    // After restarting we need to satisfy the callbacks
    MoveFromDescribeToReady();

    EXPECT_EQ(2, mPutStreamFuncCount);
    EXPECT_EQ(2, mGetStreamingEndpointFuncCount);
    EXPECT_EQ(2, mGetStreamingTokenFuncCount);
    EXPECT_EQ(2, mDescribeStreamFuncCount);

    // Put frame again to ensure there are no more callbacks
    frame.index++;
    frame.decodingTs += TEST_LONG_FRAME_DURATION;
    frame.presentationTs += TEST_LONG_FRAME_DURATION;
    frame.flags = FRAME_FLAG_KEY_FRAME;
    EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

    EXPECT_EQ(2, mPutStreamFuncCount);
    EXPECT_EQ(2, mGetStreamingEndpointFuncCount);
    EXPECT_EQ(2, mGetStreamingTokenFuncCount);
    EXPECT_EQ(2, mDescribeStreamFuncCount);
}

TEST_F(StreamApiFunctionalityTest, putFrame_PutGetRestartBadResult)
{
    UINT32 i, filledSize;
    BYTE tempBuffer[10000];
    BYTE getDataBuffer[20000];
    UINT64 timestamp, clientStreamHandle;
    Frame frame;

    // Create and ready a stream
    ReadyStream();

    frame.duration = TEST_LONG_FRAME_DURATION;
    frame.size = SIZEOF(tempBuffer);
    frame.frameData = tempBuffer;
    for (i = 0, timestamp = 0; timestamp < TEST_BUFFER_DURATION; timestamp += TEST_LONG_FRAME_DURATION, i++) {
        frame.index = i;
        frame.decodingTs = timestamp;
        frame.presentationTs = timestamp;

        // Key frame every 10th
        frame.flags = i % 10 == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;
        EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

        // Ensure put stream is called
        EXPECT_EQ(1, mPutStreamFuncCount);
        EXPECT_EQ(0, STRCMP(TEST_STREAM_NAME, mStreamName));
        EXPECT_EQ(0, STRCMP(TEST_STREAMING_ENDPOINT, mStreamingEndpoint));
        EXPECT_EQ(TRUE, mAckRequired);
        // Need to ensure we have a streaming token in the auth
        EXPECT_EQ(SIZEOF(TEST_STREAMING_TOKEN), mCallContext.pAuthInfo->size);
        EXPECT_EQ(0, STRCMP(TEST_STREAMING_TOKEN, (PCHAR) mCallContext.pAuthInfo->data));

        // Drop frame hasn't been called
        EXPECT_EQ(0, mFrameTime);

        // Return a put stream result on 50th
        if (i == 50) {
            EXPECT_EQ(STATUS_SUCCESS, putStreamResultEvent(mCallContext.customData, SERVICE_CALL_RESULT_OK, TEST_STREAMING_HANDLE));
        }
    }

    // Now try to retrieve the data
    for (timestamp = 0; timestamp < TEST_BUFFER_DURATION / 2; timestamp += TEST_LONG_FRAME_DURATION) {
        clientStreamHandle = 0;
        EXPECT_EQ(STATUS_SUCCESS,
                  getKinesisVideoStreamData(mStreamHandle, &clientStreamHandle, getDataBuffer, SIZEOF(getDataBuffer),
                                            &filledSize));
        EXPECT_EQ(SIZEOF(getDataBuffer), filledSize);
        EXPECT_EQ(TEST_STREAMING_HANDLE, clientStreamHandle);
    }

    // Send a terminate event to warm reset the stream
    EXPECT_EQ(STATUS_SUCCESS, kinesisVideoStreamTerminated(mCallContext.customData, TEST_STREAMING_HANDLE, SERVICE_CALL_INVALID_ARG));

    // Try streaming again and ensure the right callbacks are fired
    frame.index++;
    frame.decodingTs += TEST_LONG_FRAME_DURATION;
    frame.presentationTs += TEST_LONG_FRAME_DURATION;
    frame.flags = FRAME_FLAG_KEY_FRAME;
    EXPECT_EQ(STATUS_SERVICE_CALL_INVALID_ARG_ERROR, putKinesisVideoFrame(mStreamHandle, &frame));
}

TEST_F(StreamApiFunctionalityTest, putFrame_PutGetRestartBadResultRestart)
{
    UINT32 i, filledSize;
    BYTE tempBuffer[10000];
    BYTE getDataBuffer[20000];
    UINT64 timestamp, clientStreamHandle;
    Frame frame;

    // Set to restart
    mStreamInfo.streamCaps.recoverOnError = TRUE;
    // Create and ready a stream
    ReadyStream();

    frame.duration = TEST_LONG_FRAME_DURATION;
    frame.size = SIZEOF(tempBuffer);
    frame.frameData = tempBuffer;
    for (i = 0, timestamp = 0; timestamp < TEST_BUFFER_DURATION; timestamp += TEST_LONG_FRAME_DURATION, i++) {
        frame.index = i;
        frame.decodingTs = timestamp;
        frame.presentationTs = timestamp;

        // Key frame every 10th
        frame.flags = i % 10 == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;
        EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

        // Ensure put stream is called
        EXPECT_EQ(1, mPutStreamFuncCount);
        EXPECT_EQ(0, STRCMP(TEST_STREAM_NAME, mStreamName));
        EXPECT_EQ(0, STRCMP(TEST_STREAMING_ENDPOINT, mStreamingEndpoint));
        EXPECT_EQ(TRUE, mAckRequired);
        // Need to ensure we have a streaming token in the auth
        EXPECT_EQ(SIZEOF(TEST_STREAMING_TOKEN), mCallContext.pAuthInfo->size);
        EXPECT_EQ(0, STRCMP(TEST_STREAMING_TOKEN, (PCHAR) mCallContext.pAuthInfo->data));

        // Drop frame hasn't been called
        EXPECT_EQ(0, mFrameTime);

        // Return a put stream result on 50th
        if (i == 50) {
            EXPECT_EQ(STATUS_SUCCESS, putStreamResultEvent(mCallContext.customData, SERVICE_CALL_RESULT_OK, TEST_STREAMING_HANDLE));
        }
    }

    // Now try to retrieve the data
    for (timestamp = 0; timestamp < TEST_BUFFER_DURATION / 2; timestamp += TEST_LONG_FRAME_DURATION) {
        clientStreamHandle = 0;
        EXPECT_EQ(STATUS_SUCCESS,
                  getKinesisVideoStreamData(mStreamHandle, &clientStreamHandle, getDataBuffer, SIZEOF(getDataBuffer),
                                            &filledSize));
        EXPECT_EQ(SIZEOF(getDataBuffer), filledSize);
        EXPECT_EQ(TEST_STREAMING_HANDLE, clientStreamHandle);
    }

    // Send a terminate event to warm reset the stream
    EXPECT_EQ(STATUS_SUCCESS, kinesisVideoStreamTerminated(mCallContext.customData, TEST_STREAMING_HANDLE, SERVICE_CALL_INVALID_ARG));

    // Try streaming again - should succeed
    frame.index++;
    frame.decodingTs += TEST_LONG_FRAME_DURATION;
    frame.presentationTs += TEST_LONG_FRAME_DURATION;
    frame.flags = FRAME_FLAG_KEY_FRAME;
    EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

    // Try streaming again - should succeed
    frame.index++;
    frame.decodingTs += TEST_LONG_FRAME_DURATION;
    frame.presentationTs += TEST_LONG_FRAME_DURATION;
    frame.flags = FRAME_FLAG_KEY_FRAME;
    EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));
}

TEST_F(StreamApiFunctionalityTest, putFrame_StreamDataAvailable)
{
    BYTE tempBuffer[10000];
    UINT64 timestamp = 100;
    Frame frame;

    // Create and ready a stream
    ReadyStream();

    // Ensure the stream data available hasn't been called
    EXPECT_EQ(0, mStreamDataAvailableFuncCount);
    EXPECT_EQ(0, mDataReadySize);
    EXPECT_EQ(0, mDataReadyDuration);

    frame.index = 0;
    frame.decodingTs = timestamp;
    frame.presentationTs = timestamp;
    frame.duration = TEST_FRAME_DURATION;
    frame.size = SIZEOF(tempBuffer);
    frame.frameData = tempBuffer;
    frame.flags = FRAME_FLAG_KEY_FRAME;

    EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

    // Ensure put stream is called
    EXPECT_EQ(0, STRCMP(TEST_STREAM_NAME, mStreamName));
    EXPECT_EQ(TRUE, mAckRequired);
    // Need to ensure we have a streaming token in the auth
    EXPECT_EQ(SIZEOF(TEST_STREAMING_TOKEN), mCallContext.pAuthInfo->size);
    EXPECT_EQ(0, STRCMP(TEST_STREAMING_TOKEN, (PCHAR) mCallContext.pAuthInfo->data));

    // Validate the function has NOT been called as we haven't yet executed putStreamResultEvent
    EXPECT_EQ(0, mStreamDataAvailableFuncCount);
    EXPECT_EQ(0, mDataReadyDuration);
    EXPECT_EQ(0, mDataReadySize);

    // Set the result
    EXPECT_EQ(STATUS_SUCCESS, putStreamResultEvent(mCallContext.customData, SERVICE_CALL_RESULT_OK, TEST_STREAMING_HANDLE));

    // Call again with cleared data
    mStreamName[0] = '\0';
    mAckRequired = FALSE;

    frame.index = 1;
    frame.decodingTs = timestamp + TEST_FRAME_DURATION;
    frame.presentationTs = timestamp + TEST_FRAME_DURATION;
    frame.duration = TEST_FRAME_DURATION;
    frame.size = SIZEOF(tempBuffer);
    frame.frameData = tempBuffer;
    frame.flags = FRAME_FLAG_KEY_FRAME;
    EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

    // Validate that StreamDataAvailableFn was invoked.
    EXPECT_EQ(0, STRCMP(TEST_STREAM_NAME, mStreamName));
    EXPECT_EQ(FALSE, mAckRequired);

    // Validate the function has been called
    EXPECT_EQ(1, mStreamDataAvailableFuncCount);
    EXPECT_EQ(TEST_FRAME_DURATION * 2, mDataReadyDuration);

    // Should be encoded size
    EXPECT_EQ(SIZEOF(tempBuffer) + MKV_HEADER_OVERHEAD + SIZEOF(tempBuffer) + MKV_SIMPLE_BLOCK_OVERHEAD, mDataReadySize);
}

TEST_F(StreamApiFunctionalityTest, putFrame_AdaptAnnexB)
{
    BYTE frameData[] = {0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x1e,
                        0xa9, 0x50, 0x14, 0x07, 0xb4, 0x20, 0x00, 0x00,
                        0x7d, 0x00, 0x00, 0x1d, 0x4c, 0x00, 0x80, 0x00,
                        0x00, 0x00, 0x01, 0x68, 0xce, 0x3c, 0x80};
    UINT32 frameDataSize = SIZEOF(frameData);
    Frame frame;

    // Create and ready a stream
    ReadyStream();

    frame.index = 0;
    frame.decodingTs = 0;
    frame.presentationTs = 0;
    frame.duration = TEST_FRAME_DURATION;
    frame.size = frameDataSize;
    frame.frameData = frameData;
    frame.flags = FRAME_FLAG_KEY_FRAME;
    EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));
}

TEST_F(StreamApiFunctionalityTest, PutGet_ConnectionStaleNotification)
{
    UINT32 i, filledSize;
    BYTE tempBuffer[10000];
    BYTE getDataBuffer[20000];
    UINT64 timestamp, delay, clientStreamHandle;
    Frame frame;
    STATUS retStatus = STATUS_SUCCESS;

    // Set the connection staleness threshold
    delay = TEST_LONG_FRAME_DURATION * 30;
    mStreamInfo.streamCaps.connectionStalenessDuration = delay;
    mStreamInfo.streamCaps.fragmentAcks = true;

    // Create and ready a stream
    ReadyStream();

    for (i = 0, timestamp = 0; timestamp <= TEST_BUFFER_DURATION; timestamp += TEST_LONG_FRAME_DURATION, i++) {
        frame.index = i;
        frame.decodingTs = timestamp;
        frame.presentationTs = timestamp;
        frame.duration = TEST_LONG_FRAME_DURATION;
        frame.size = SIZEOF(tempBuffer);
        frame.frameData = tempBuffer;

        // Key frame every 10th
        frame.flags = i % 10 == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;
        EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(mStreamHandle, &frame));

        // Return a put stream result on 1st
        if (i == 1) {
           EXPECT_EQ(STATUS_SUCCESS, putStreamResultEvent(mCallContext.customData, SERVICE_CALL_RESULT_OK, TEST_STREAMING_HANDLE));
        }

        if (i >= 1) {
            clientStreamHandle = 0;
            retStatus = getKinesisVideoStreamData(mStreamHandle, &clientStreamHandle, getDataBuffer, SIZEOF(getDataBuffer),
                    &filledSize);
            EXPECT_EQ(true, STATUS_SUCCESS == retStatus || STATUS_NO_MORE_DATA_AVAILABLE == retStatus);
            EXPECT_EQ(true, filledSize > 0);
            EXPECT_EQ(TEST_STREAMING_HANDLE, clientStreamHandle);
        }

        if (timestamp == delay + TEST_LONG_FRAME_DURATION) {
            // Should have called a pressure notification
            EXPECT_EQ(1, mStreamConnectionStaleFuncCount);
            EXPECT_EQ(delay + TEST_LONG_FRAME_DURATION, mDuration);
        } else if (timestamp == TEST_BUFFER_DURATION) {
            EXPECT_EQ(i - delay / TEST_LONG_FRAME_DURATION, mStreamConnectionStaleFuncCount);
            EXPECT_EQ(delay + TEST_LONG_FRAME_DURATION, mDuration);
        } else if (timestamp < delay) {
            // Shouldn't have been called yet
            EXPECT_EQ(0, mStreamConnectionStaleFuncCount);
            EXPECT_EQ(0, mDuration);
        }
    }
}
