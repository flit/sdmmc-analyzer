#include "SDMMCAnalyzerResults.h"
#include "SDMMCAnalyzer.h"
#include "SDMMCAnalyzerSettings.h"
#include "SDMMCHelpers.h"

static const char * const kReplyStateNames[] = {
           /* 0 */ "Idle",
           /* 1 */ "Ready",
           /* 2 */ "Ident",
           /* 3 */ "Stby",
           /* 4 */ "Tran",
           /* 5 */ "Data",
           /* 6 */ "Rcv",
           /* 7 */ "Prg",
           /* 8 */ "Dis",
           /* 9 */ "Btst",
           /* a */ "Slp",
           /* b */ "reserved",
           /* c */ "reserved",
           /* d */ "reserved",
           /* e */ "reserved",
           /* f */ "reserved",
        };

SDMMCAnalyzerResults::SDMMCAnalyzerResults(SDMMCAnalyzer* analyzer, SDMMCAnalyzerSettings* settings)
:	AnalyzerResults(),
	mSettings(settings),
	mAnalyzer(analyzer)
{
}

SDMMCAnalyzerResults::~SDMMCAnalyzerResults()
{
}

void SDMMCAnalyzerResults::GenerateBubbleText(U64 frame_index, Channel& channel, DisplayBase display_base)
{
	ClearResultStrings();
	Frame frame = GetFrame(frame_index);

    char str_cmd[4];
	switch (frame.mType) {
	case FRAMETYPE_COMMAND:
	{
		char str_arg[33];

		AnalyzerHelpers::GetNumberString(frame.mData1, Decimal, 6, str_cmd, sizeof(str_cmd));
		AnalyzerHelpers::GetNumberString(frame.mData2, display_base, 32, str_arg, sizeof(str_arg));

		AddResultString("CMD");
		AddResultString("CMD", str_cmd);
		AddResultString("CMD", str_cmd, ", arg=", str_arg);
		break;
	}

    case FRAMETYPE_RESPONSE:
    {
        const char *responseType;
        switch (frame.mFlags) {
        case MMC_RSP_R1:
            responseType = "R1";
            break;
        case MMC_RSP_R2_CID:
            responseType = "R2";
            break;
        case MMC_RSP_R2_CSD:
            responseType = "R2";
            break;
        case MMC_RSP_R3:
            responseType = "R3";
            break;
        case MMC_RSP_R4:
            responseType = "R4";
            break;
        case MMC_RSP_R5:
            responseType = "R5";
            break;
        }

        AnalyzerHelpers::GetNumberString(frame.mData1, Decimal, 32, str_cmd, sizeof(str_cmd));

        AddResultString(responseType);
        AddResultString(responseType, " CMD", str_cmd);

        break;
    }

	case FRAMETYPE_RESPONSE_DATA:
	{
		char str_32[33];

		switch (frame.mFlags) {
		case MMC_RSP_R1:
		{
			const char *str_state = kReplyStateNames[((frame.mData1 >> 9) & 0xf)];
            std::string str_flags = GetResponseFlagsDescription(frame);

			AnalyzerHelpers::GetNumberString(frame.mData1, display_base, 32, str_32, sizeof(str_32));
			
			AddResultString("Status");
			AddResultString("Status: ", str_state);
			AddResultString("Status: ", str_state, ", rsp=", str_32);

			if (str_flags.length() > 0)
				AddResultString("Status: ", str_state, ", rsp=", str_32, str_flags.c_str());

			break;
		}
		case MMC_RSP_R2_CID:
		{
			std::string res("[CID]");
			char pname[7], prv_str[4], psn_str[12];
			char rsp_str[64];

			AddResultString(res.c_str());

			res += " rsp=";
			AnalyzerHelpers::GetNumberString(frame.mData1 >> 32, display_base, 32, rsp_str, sizeof(rsp_str));
			res += rsp_str;
			res += " ";
			AnalyzerHelpers::GetNumberString(frame.mData1 & 0xffffffffull, display_base, 32, rsp_str, sizeof(rsp_str));
			res += rsp_str;
			res += " ";
			AnalyzerHelpers::GetNumberString(frame.mData2 >> 32, display_base, 32, rsp_str, sizeof(rsp_str));
			res += rsp_str;
			res += " ";
			AnalyzerHelpers::GetNumberString(frame.mData2 & 0xffffffffull, display_base, 32, rsp_str, sizeof(rsp_str));
			res += rsp_str;
			AddResultString(res.c_str());

			pname[0] = (frame.mData1 >> 32) & 0xff;
			pname[1] = (frame.mData1 >> 24) & 0xff;
			pname[2] = (frame.mData1 >> 16) & 0xff;
			pname[3] = (frame.mData1 >>  8) & 0xff;
			pname[4] = (frame.mData1 >>  0) & 0xff;
			pname[5] = (frame.mData2 >> 56) & 0xff;
			pname[6] = 0;

			unsigned prv = (unsigned)((frame.mData2 >> 48) & 0xff);
			prv_str[0] = '0' + ((prv >> 4) & 0x0f);
			prv_str[1] = '.';
			prv_str[2] = '0' + (prv & 0x0f);
			prv_str[3] = 0;

			unsigned psn = (unsigned)((frame.mData2 >> 16) & 0xfffffffful);
			AnalyzerHelpers::GetNumberString(psn, Decimal, 32, psn_str, sizeof(psn_str));

			res += " pnm='";
			res += pname;
			res += "' prv=";
			res += prv_str;
			res += " psn=";
			res += psn_str;
			AddResultString(res.c_str());

			break;
		}
		case MMC_RSP_R2_CSD:
		{
			std::string res("[CSD]");
			char rsp_str[64];

			AddResultString(res.c_str());

//            res += " [CSD]";
//            AddResultString(res.c_str());

			res += " rsp=";
			AnalyzerHelpers::GetNumberString(frame.mData1 >> 32, display_base, 32, rsp_str, sizeof(rsp_str));
			res += rsp_str;
			res += " ";
			AnalyzerHelpers::GetNumberString(frame.mData1 & 0xffffffffull, display_base, 32, rsp_str, sizeof(rsp_str));
			res += rsp_str;
			res += " ";
			AnalyzerHelpers::GetNumberString(frame.mData2 >> 32, display_base, 32, rsp_str, sizeof(rsp_str));
			res += rsp_str;
			res += " ";
			AnalyzerHelpers::GetNumberString(frame.mData2 & 0xffffffffull, display_base, 32, rsp_str, sizeof(rsp_str));
			res += rsp_str;
			AddResultString(res.c_str());

			break;
		}
		case MMC_RSP_R3:
			AnalyzerHelpers::GetNumberString(frame.mData1, Hexadecimal, 32, str_32, sizeof(str_32));
            AddResultString("ocr");
			AddResultString("ocr=", str_32);
			break;
		case MMC_RSP_R4:
			AddResultString("R4");
			break;
		case MMC_RSP_R5:
			AddResultString("R5");
			break;
		}
		break;
	}

	case FRAMETYPE_CRC:
	{
		char str_crc[8];

		AnalyzerHelpers::GetNumberString(frame.mData1, Hexadecimal, 7, str_crc, sizeof(str_crc));

		AddResultString("CRC");
		AddResultString("CRC=", str_crc);

//        if (frame.mData2 == 1) {
//            AddResultString("CRC=", str_crc, " [invalid!]");
//        }
//        else {
//            AddResultString("CRC=", str_crc, " [good]");
//        }
		break;
	}

	default:
		AddResultString("error");
	}
}

void SDMMCAnalyzerResults::GenerateExportFile(const char* file, DisplayBase display_base, U32 export_type_user_id)
{
	ClearResultStrings();
	AddResultString("not supported");
}

void SDMMCAnalyzerResults::GenerateFrameTabularText(U64 frame_index, DisplayBase display_base)
{
	ClearTabularText();
    Frame frame = GetFrame(frame_index);

    char str_cmd[4];
    switch (frame.mType) {
    case FRAMETYPE_COMMAND:
    {
        char str_arg[33];

        AnalyzerHelpers::GetNumberString(frame.mData1, Decimal, 6, str_cmd, sizeof(str_cmd));
        AnalyzerHelpers::GetNumberString(frame.mData2, display_base, 32, str_arg, sizeof(str_arg));

        AddTabularText("CMD", str_cmd, ", arg=", str_arg);
        break;
    }

    case FRAMETYPE_RESPONSE:
        const char *responseType;
        switch (frame.mFlags) {
        case MMC_RSP_R1:
            responseType = "R1";
            break;
        case MMC_RSP_R2_CID:
            responseType = "R2";
            break;
        case MMC_RSP_R2_CSD:
            responseType = "R2";
            break;
        case MMC_RSP_R3:
            responseType = "R3";
            break;
        case MMC_RSP_R4:
            responseType = "R4";
            break;
        case MMC_RSP_R5:
            responseType = "R5";
            break;
        }

        AnalyzerHelpers::GetNumberString(frame.mData1, Decimal, 32, str_cmd, sizeof(str_cmd));

        AddTabularText(responseType, " CMD", str_cmd);
        break;

    case FRAMETYPE_RESPONSE_DATA:
    {
        char str_32[33];

        switch (frame.mFlags) {
        case MMC_RSP_R1:
        {
            const char *str_state = kReplyStateNames[((frame.mData1 >> 9) & 0xf)];
            std::string str_flags = GetResponseFlagsDescription(frame);

            AnalyzerHelpers::GetNumberString(frame.mData1, display_base, 32, str_32, sizeof(str_32));

            if (str_flags.length() > 0) {
                AddTabularText("Status: ", str_state, ", rsp=", str_32, str_flags.c_str());
            }
            else {
                AddTabularText("Status: ", str_state, ", rsp=", str_32);
            }

            break;
        }
        case MMC_RSP_R2_CID:
        {
            std::string res("[CID]");
            char pname[7], prv_str[4], psn_str[12];
            char rsp_str[64];

            res += " rsp=";
            AnalyzerHelpers::GetNumberString(frame.mData1 >> 32, display_base, 32, rsp_str, sizeof(rsp_str));
            res += rsp_str;
            res += " ";
            AnalyzerHelpers::GetNumberString(frame.mData1 & 0xffffffffull, display_base, 32, rsp_str, sizeof(rsp_str));
            res += rsp_str;
            res += " ";
            AnalyzerHelpers::GetNumberString(frame.mData2 >> 32, display_base, 32, rsp_str, sizeof(rsp_str));
            res += rsp_str;
            res += " ";
            AnalyzerHelpers::GetNumberString(frame.mData2 & 0xffffffffull, display_base, 32, rsp_str, sizeof(rsp_str));
            res += rsp_str;

            pname[0] = (frame.mData1 >> 32) & 0xff;
            pname[1] = (frame.mData1 >> 24) & 0xff;
            pname[2] = (frame.mData1 >> 16) & 0xff;
            pname[3] = (frame.mData1 >>  8) & 0xff;
            pname[4] = (frame.mData1 >>  0) & 0xff;
            pname[5] = (frame.mData2 >> 56) & 0xff;
            pname[6] = 0;

            unsigned prv = (unsigned)((frame.mData2 >> 48) & 0xff);
            prv_str[0] = '0' + ((prv >> 4) & 0x0f);
            prv_str[1] = '.';
            prv_str[2] = '0' + (prv & 0x0f);
            prv_str[3] = 0;

            unsigned psn = (unsigned)((frame.mData2 >> 16) & 0xfffffffful);
            AnalyzerHelpers::GetNumberString(psn, Decimal, 32, psn_str, sizeof(psn_str));

            res += " pnm='";
            res += pname;
            res += "' prv=";
            res += prv_str;
            res += " psn=";
            res += psn_str;
            AddTabularText(res.c_str());

            break;
        }
        case MMC_RSP_R2_CSD:
        {
            std::string res("[CSD]");
            char rsp_str[64];

            res += " rsp=";
            AnalyzerHelpers::GetNumberString(frame.mData1 >> 32, display_base, 32, rsp_str, sizeof(rsp_str));
            res += rsp_str;
            res += " ";
            AnalyzerHelpers::GetNumberString(frame.mData1 & 0xffffffffull, display_base, 32, rsp_str, sizeof(rsp_str));
            res += rsp_str;
            res += " ";
            AnalyzerHelpers::GetNumberString(frame.mData2 >> 32, display_base, 32, rsp_str, sizeof(rsp_str));
            res += rsp_str;
            res += " ";
            AnalyzerHelpers::GetNumberString(frame.mData2 & 0xffffffffull, display_base, 32, rsp_str, sizeof(rsp_str));
            res += rsp_str;
            AddTabularText(res.c_str());

            break;
        }
        case MMC_RSP_R3:
            AnalyzerHelpers::GetNumberString(frame.mData1, Hexadecimal, 32, str_32, sizeof(str_32));
            AddTabularText("ocr=", str_32);
            break;
        case MMC_RSP_R4:
            AddTabularText("R4");
            break;
        case MMC_RSP_R5:
            AddTabularText("R5");
            break;
        }
        break;
    }

    case FRAMETYPE_CRC:
    {
        char str_crc[8];

        AnalyzerHelpers::GetNumberString(frame.mData1, Hexadecimal, 7, str_crc, sizeof(str_crc));

        AddTabularText("CRC=", str_crc);

//        if (frame.mData2 == 1) {
//            AddTabularText("CRC=", str_crc, " [invalid!]");
//        }
//        else {
//            AddTabularText("CRC=", str_crc, " [good]");
//        }
        break;
    }

    default:
        break;
    }
}

std::string SDMMCAnalyzerResults::GetResponseFlagsDescription(const Frame& frame)
{
    std::string str_flags("");
    if (frame.mData1 & (1 << 31))
        str_flags += " ADDRESS_OUT_OF_RANGE";
    if (frame.mData1 & (1 << 30))
        str_flags += " ADDRESS_MISALIGN";
    if (frame.mData1 & (1 << 29))
        str_flags += " BLOCK_LEN_ERROR";
    if (frame.mData1 & (1 << 28))
        str_flags += " ERASE_SEQ_ERROR";
    if (frame.mData1 & (1 << 27))
        str_flags += " ERASE_PARAM";
    if (frame.mData1 & (1 << 26))
        str_flags += " WP_VIOLATION";
    if (frame.mData1 & (1 << 25))
        str_flags += " CARD_IS_LOCKED";
    if (frame.mData1 & (1 << 24))
        str_flags += " LOCK_UNLOCK_FAILED";
    if (frame.mData1 & (1 << 23))
        str_flags += " COM_CRC_ERROR";
    if (frame.mData1 & (1 << 22))
        str_flags += " ILLEGAL_COMMAND";
    if (frame.mData1 & (1 << 21))
        str_flags += " CARD_ECC_FAILED";
    if (frame.mData1 & (1 << 20))
        str_flags += " CC_ERROR";
    if (frame.mData1 & (1 << 19))
        str_flags += " ERROR";
    if (frame.mData1 & (1 << 18))
        str_flags += " UNDERRUN";
    if (frame.mData1 & (1 << 17))
        str_flags += " OVERRUN";
    if (frame.mData1 & (1 << 16))
        str_flags += " CID/CSD_OVERWRITE";
    if (frame.mData1 & (1 << 15))
        str_flags += " WP_ERASE_SKIP";
    if (frame.mData1 & (1 << 13))
        str_flags += " ERASE_RESET";
    if (frame.mData1 & (1 << 8))
        str_flags += " READY_FOR_DATA";
    if (frame.mData1 & (1 << 7))
        str_flags += " SWITCH_ERROR";
    if (frame.mData1 & (1 << 5))
        str_flags += " APP_CMD";
    return str_flags;
}

void SDMMCAnalyzerResults::GeneratePacketTabularText(U64 packet_id, DisplayBase display_base)
{
	ClearResultStrings();
	AddResultString("not supported");
}

void SDMMCAnalyzerResults::GenerateTransactionTabularText(U64 transaction_id, DisplayBase display_base)
{
	ClearResultStrings();
	AddResultString("not supported");
}
