#include <AnalyzerChannelData.h>
#include "SDMMCAnalyzer.h"
#include "SDMMCAnalyzerResults.h"
#include "SDMMCHelpers.h"

const char SDMMCAnalyzer::Name[] = "SDMMC";

SDMMCAnalyzer::SDMMCAnalyzer()
:	Analyzer2(),
	mSettings(new SDMMCAnalyzerSettings()),
	mSimulationInitialized(false)
{
	SetAnalyzerSettings(mSettings.get());
}

SDMMCAnalyzer::~SDMMCAnalyzer()
{
	KillThread();
}

const char* SDMMCAnalyzer::GetAnalyzerName() const
{
	return Name;
}

void SDMMCAnalyzer::SetupResults()
{
    mResults.reset(new SDMMCAnalyzerResults(this, mSettings.get()));
    SetAnalyzerResults(mResults.get());

    mResults->AddChannelBubblesWillAppearOn(mSettings->mCommandChannel);
}

void SDMMCAnalyzer::WorkerThread()
{
	mClock = GetAnalyzerChannelData(mSettings->mClockChannel);
	mCommand = GetAnalyzerChannelData(mSettings->mCommandChannel);

	while (true) {
		int cmdindex;

		ReportProgress(mClock->GetSampleNumber());
		CheckIfThreadShouldExit();
		AdvanceToNextClock();
		
		cmdindex = TryReadCommand();
		if (cmdindex < 0) {
			/* continue if parsing the command failed */
			continue;
		}

		if (mSettings->mProtocol == PROTOCOL_MMC) {
			struct MMCResponse response = SDMMCHelpers::MMCCommandResponse(cmdindex);
			if (response.mType != MMC_RSP_NONE)
				WaitForAndReadMMCResponse(response);
		} else {
			/* FIXME: implement unique SD response handling */
            struct MMCResponse response = SDMMCHelpers::MMCCommandResponse(cmdindex);
            if (response.mType != MMC_RSP_NONE)
                WaitForAndReadMMCResponse(response);
		}

        mResults->CommitPacketAndStartNewPacket();
        mResults->CommitResults();
	}
}

bool SDMMCAnalyzer::NeedsRerun()
{
	return false;
}

U32 SDMMCAnalyzer::GenerateSimulationData(U64 newest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channels)
{
	if (!mSimulationInitialized) {
	mDataGenerator.Initialize(GetSimulationSampleRate(), mSettings.get());
	mSimulationInitialized = true;
	}

	return mDataGenerator.GenerateSimulationData(newest_sample_requested, sample_rate, simulation_channels);
}

U32 SDMMCAnalyzer::GetMinimumSampleRateHz()
{
	return 400000 * 4;
}

void SDMMCAnalyzer::AdvanceToNextClock()
{
	enum BitState search = mSettings->mSampleEdge == SAMPLE_EDGE_RISING ? BIT_HIGH : BIT_LOW;

	do {
		mClock->AdvanceToNextEdge();
	} while (mClock->GetBitState() != search);

    mResults->AddMarker(mClock->GetSampleNumber(), (mSettings->mSampleEdge == SAMPLE_EDGE_RISING) ?  AnalyzerResults::UpArrow : AnalyzerResults::DownArrow, mSettings->mClockChannel);

	mCommand->AdvanceToAbsPosition(mClock->GetSampleNumber());
}

int SDMMCAnalyzer::TryReadCommand()
{
	int index;
	
	/* check for start bit */
	if (mCommand->GetBitState() != BIT_LOW)
		return -1;

    U64 commandStartSample = mClock->GetSampleNumber();
	mResults->AddMarker(commandStartSample, AnalyzerResults::Start, mSettings->mCommandChannel);
	AdvanceToNextClock();
	
	/* transfer bit */
	if (mCommand->GetBitState() != BIT_HIGH) {
		/* if host is not transferring this is no command */
		mResults->AddMarker(mClock->GetSampleNumber(), AnalyzerResults::X, mSettings->mCommandChannel);
		return -1;
	}
	AdvanceToNextClock();

	/* command index and argument */
    Frame commandFrame;
	{
		Frame frame;

		frame.mStartingSampleInclusive = commandStartSample;
		frame.mData1 = 0;
		frame.mData2 = 0;
		frame.mType = SDMMCAnalyzerResults::FRAMETYPE_COMMAND;

        // read 6-bit command index
		for (int i = 0; i < 6; i++) {
			frame.mData1 = (frame.mData1 << 1) | mCommand->GetBitState();
			AdvanceToNextClock();
		}

        // read 32-bit command argument
		for (int i = 0; i < 32; i++) {
			frame.mData2 = (frame.mData2 << 1) | mCommand->GetBitState();
			AdvanceToNextClock();
		}

		frame.mEndingSampleInclusive = mClock->GetSampleNumber() - 1;
		mResults->AddFrame(frame);
        commandFrame = frame;
		
		/* save index for returning */
		index = (int)frame.mData1;
	}

	/* crc */
	{
		Frame frame;

		frame.mStartingSampleInclusive = mClock->GetSampleNumber();
		frame.mData1 = 0;
        frame.mData2 = 0; // is bad
		frame.mType = SDMMCAnalyzerResults::FRAMETYPE_CRC;

		for (int i = 0; i < 7; i++) {
			frame.mData1 = (frame.mData1 << 1) | mCommand->GetBitState();
			AdvanceToNextClock();
		}

        // check crc. for commands the crc is always computed over 40 bits.
//        U8 crcBuffer[5];
//        crcBuffer[0] = 0x40 | commandFrame.mData1; // start bit, transfer bit, cmd index
//        crcBuffer[1] = (commandFrame.mData2 >> 24) & 0xff;
//        crcBuffer[2] = (commandFrame.mData2 >> 16) & 0xff;
//        crcBuffer[3] = (commandFrame.mData2 >> 8) & 0xff;
//        crcBuffer[4] = commandFrame.mData2 & 0xff;
//        U8 crc7 = SDMMCHelpers::crc7(&crcBuffer[0], sizeof(crcBuffer));

//        if (crc7 != frame.mData1) {
//            frame.mFlags |= DISPLAY_AS_ERROR_FLAG;
//            frame.mData2 = 1;
//        }

		frame.mEndingSampleInclusive = mClock->GetSampleNumber() - 1;
		mResults->AddFrame(frame);
	}

	/* stop bit */
	mResults->AddMarker(mClock->GetSampleNumber(), AnalyzerResults::Stop, mSettings->mCommandChannel);

	mResults->CommitResults();
	ReportProgress(mClock->GetSampleNumber());
	
	return index;
}

int SDMMCAnalyzer::WaitForAndReadMMCResponse(const struct MMCResponse& response)
{
    int i;
	int timeout = response.mTimeout + 3; // add some slack time

	while (timeout-- >= 0 && mCommand->GetBitState() != BIT_LOW) {
		AdvanceToNextClock();
    }

	if (timeout < 0) {
		return -1;
    }

    // response
    U8 responseCommandIndex = 0;
    {
        U64 responseStartSample = mClock->GetSampleNumber();

        Frame frame;
        frame.mStartingSampleInclusive = responseStartSample;
        frame.mType = SDMMCAnalyzerResults::FRAMETYPE_RESPONSE;
        frame.mFlags = response.mType;

        mResults->AddMarker(responseStartSample, AnalyzerResults::Start, mSettings->mCommandChannel);
        AdvanceToNextClock();

        /* transfer bit */
        if (mCommand->GetBitState() != BIT_LOW) {
            /* if card is not transferring this is no response */
            mResults->AddMarker(mClock->GetSampleNumber(), AnalyzerResults::X, mSettings->mCommandChannel);
            return -1;
        }
        AdvanceToNextClock();

        /* read 6 bit command index */
        for (i = 0; i < 6; i++) {
            responseCommandIndex = (responseCommandIndex << 1) | mCommand->GetBitState();
            AdvanceToNextClock();
        }
        frame.mData1 = responseCommandIndex;

        frame.mEndingSampleInclusive = mClock->GetSampleNumber() - 1;
        mResults->AddFrame(frame);
    }

	/* response data */
    Frame responseFrame;
	{
		Frame frame;

		frame.mStartingSampleInclusive = mClock->GetSampleNumber();
		frame.mData1 = 0;
		frame.mData2 = 0;
		frame.mType = SDMMCAnalyzerResults::FRAMETYPE_RESPONSE_DATA;
		frame.mFlags = response.mType;

		int bits = response.mBits;

		for (i = 0; i < 64 && bits > 0; i++, bits--) {
			frame.mData1 = (frame.mData1 << 1) | mCommand->GetBitState();
			AdvanceToNextClock();
		}

		for (i = 0; i < 64 && bits > 0; i++, bits--) {
			frame.mData2 = (frame.mData2 << 1) | mCommand->GetBitState();
			AdvanceToNextClock();
		}

		frame.mEndingSampleInclusive = mClock->GetSampleNumber() - 1;
		mResults->AddFrame(frame);
        responseFrame = frame;
	}

	/* crc */
	if (response.mType != MMC_RSP_R2_CID && response.mType != MMC_RSP_R2_CSD) {
		Frame frame;

		frame.mStartingSampleInclusive = mClock->GetSampleNumber();
		frame.mData1 = 0;
        frame.mData2 = 0; // is bad
		frame.mType = SDMMCAnalyzerResults::FRAMETYPE_CRC;

		for (i = 0; i < 7; i++) {
			frame.mData1 = (frame.mData1 << 1) | mCommand->GetBitState();
			AdvanceToNextClock();
		}

        // check crc, always computed over 40 bits.
//        U8 crcBuffer[5];
//        crcBuffer[0] = 0x00 | responseCommandIndex; // start bit, transfer bit, cmd index
//        crcBuffer[1] = (responseFrame.mData1 >> 24) & 0xff;
//        crcBuffer[2] = (responseFrame.mData1 >> 16) & 0xff;
//        crcBuffer[3] = (responseFrame.mData1 >> 8) & 0xff;
//        crcBuffer[4] = responseFrame.mData1 & 0xff;
//        U8 crc7 = SDMMCHelpers::crc7(&crcBuffer[0], sizeof(crcBuffer));

//        if (crc7 != frame.mData1) {
//            frame.mFlags |= DISPLAY_AS_ERROR_FLAG;
//            frame.mData2 = 1;
//        }

		frame.mEndingSampleInclusive = mClock->GetSampleNumber() - 1;
		mResults->AddFrame(frame);
	}

	/* stop bit */
	mResults->AddMarker(mClock->GetSampleNumber(), AnalyzerResults::Stop, mSettings->mCommandChannel);

	mResults->CommitResults();
	ReportProgress(mClock->GetSampleNumber());
	
	return 1;
}

/*
 * loader hooks
 */

const char* GetAnalyzerName()
{
	return SDMMCAnalyzer::Name;
}

Analyzer* CreateAnalyzer()
{
	return new SDMMCAnalyzer();
}

void DestroyAnalyzer(Analyzer* analyzer)
{
	delete analyzer;
}
