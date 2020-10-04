// MotorolaHexAnalyze.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define LENGTH_SIZE									( 2 )
#define CHECK_SUM_SIZE								( 2 )
#define	NEW_LINE_SIZE								( 2 )



#define CF_WRITE_MIN_SIZE							( 128 )		// 最小プログラムサイズ(BYTE)

#define CF_CLEAR_NUM								( 0xFF )

typedef struct
{

	unsigned int									StartAddress;	// 書込み開始アドレス
	unsigned int									EndAddress;		// 書込み開始アドレス

	unsigned char									Data[CF_WRITE_MIN_SIZE];

} CF_WRITE_INFO_TABLE;



typedef enum
{
	RECORD_KIND_S0 = 0,
	RECORD_KIND_S1,
	RECORD_KIND_S2,
	RECORD_KIND_S3,
	RECORD_KIND_S7,
	RECORD_KIND_S8,
	RECORD_KIND_S9
} RECORD_KIND_ENUM;


typedef struct
{
	unsigned int			WriteNum;

	unsigned int			Address;							// ロード・アドレス
	unsigned char			DataSize;							// データ長
	unsigned char			Data[99];							// 書込みデータ
} FLASH_INFO_TABLE;



#pragma pack(0)
// S0レコード
typedef struct
{
	char					sz0E[2];							// "0E"固定
	char					sz0000[4];							// "0000"固定
	char					szFileName[22];						// "ファイル名(8文字) + ファイル形式(3文字)
	char					szChecksum[CHECK_SUM_SIZE];			// チェックサム
	char					szNewLine[NEW_LINE_SIZE];			// ニューライン
} S0_RECORD_TABLE;

// S3レコード
typedef struct
{
	char					szLength[LENGTH_SIZE];				// レコード長
	char					szAddress[8];						// ロード・アドレス(4BYTE:32bit)
	char					szCode[(0xFF * 2)];					// コード
	char					szChecksum[CHECK_SUM_SIZE];			// チェックサム
	char					szNewLine[NEW_LINE_SIZE];			// ニューライン
} S3_RECORD_TABLE;

// S7レコード
typedef struct
{
	char					szLength[LENGTH_SIZE];				// レコード長
	char					szEntryPointAddress[8];				// エントリ・ポイント・アドレス
	char					szChecksum[CHECK_SUM_SIZE];			// チェックサム
	char					szNewLine[NEW_LINE_SIZE];			// ニューライン
} S7_RECORD_TABLE;


typedef struct
{
	RECORD_KIND_ENUM		eRecordKind;				// レコード種別
	char					szRecordName[2];			// レコード名(最初の2バイトはレコード種別のため、共通項目とする)

	union
	{
		S0_RECORD_TABLE		tS0;
		S3_RECORD_TABLE		tS3;
		S7_RECORD_TABLE		tS7;
	};



} S_TYPE_RECORD_TABLE;
#pragma pack()


unsigned char atohex(const char ch)
{
	unsigned char		Hex = 0x00;


	switch (ch) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		Hex = ch - '0';
		break;
	case 'A':
	case 'a':
		Hex = 0x0A;
		break;
	case 'B':
	case 'b':
		Hex = 0x0B;
		break;
	case 'C':
	case 'c':
		Hex = 0x0C;
		break;
	case 'D':
	case 'd':
		Hex = 0x0D;
		break;
	case 'E':
	case 'e':
		Hex = 0x0E;
		break;
	case 'F':
	case 'f':
		Hex = 0x0f;
		break;
	default:
		Hex = 0x00;
		break;
	}

	return Hex;
}


//-----------------------------------------------------------------------------
// 2バイトのHEX文字を数値に変換（例："3D" → 61(0x3D)）
//-----------------------------------------------------------------------------
unsigned char StrHex2Num(const char *pszHex)
{
	unsigned char			Hex = 0x00;

	

	Hex = atohex(pszHex[0]) << 4;
	Hex += atohex(pszHex[1]);

	return Hex;
}

//-----------------------------------------------------------------------------
// 2バイトの10進数文字を数値に変換（例："91" → 91(0x5B)）
//-----------------------------------------------------------------------------
unsigned char StrNum2Num(const char* pszNum)
{
	unsigned char			Num = 0x00;

	if ((pszNum[0] >= '0') && (pszNum[0] <= '9'))
	{
		Num += pszNum[0] - '0';
	}
	Num = Num * 10;

	if ((pszNum[1] >= '0') && (pszNum[1] <= '9'))
	{
		Num += pszNum[1] - '0';
	}

	return Num;
}



//-----------------------------------------------------------------------------
// チェックサム
//-----------------------------------------------------------------------------
bool CheckSum(const char* pszData, unsigned char Size, unsigned char CheckSum)
{
	unsigned char Sum = 0x00;
	unsigned char i = 0;


	// Sum & CheckSumを算出する
	for (i = 0; i < Size ; i+=2 )
	{
		Sum += StrHex2Num(&pszData[i]);
	}
	Sum = ~Sum;

	return ((Sum == CheckSum) ? true : false);
}


//-----------------------------------------------------------------------------
// S0レコード解析
//-----------------------------------------------------------------------------
bool Analyze_S0(FILE *fp, S_TYPE_RECORD_TABLE* ptStypeRecord)
{
	size_t ReadNum = 0;
	bool bRet = false;


	// S0のレコードサイズは固定なので、S0サイズ分読み込む
	ReadNum = fread(ptStypeRecord->tS0.sz0E, 1, sizeof(S0_RECORD_TABLE), fp);
	if (ReadNum != sizeof(S0_RECORD_TABLE))
	{
		return false;
	}

	// "E0"チェック
	if ((ptStypeRecord->tS0.sz0E[0] != '0') || (ptStypeRecord->tS0.sz0E[1] != 'E'))
	{
		return false;
	}

	// "0000"チェック
	for (int i = 0; i < 4; i++)
	{
		if (ptStypeRecord->tS0.sz0000[i] != '0')
		{
			return false;
		}
	}

	// ニュー・ラインチェック
	if ((ptStypeRecord->tS0.szNewLine[0] != 0x0D) || (ptStypeRecord->tS0.szNewLine[1] != 0x0A))
	{
		return false;
	}

	// チェクサム
	bRet = CheckSum((const char*)&ptStypeRecord->tS0,
		(sizeof(S0_RECORD_TABLE) - (CHECK_SUM_SIZE + NEW_LINE_SIZE)),
		StrHex2Num((const char*)&ptStypeRecord->tS0.szChecksum));
	if (bRet == false)
	{
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// S3レコード解析
//-----------------------------------------------------------------------------
bool Analyze_S3(FILE* fp, S_TYPE_RECORD_TABLE* ptStypeRecord, FLASH_INFO_TABLE* ptFlashInfo)
{
	size_t ReadNum = 0;
	bool bRet = false;
	unsigned char Length = 0;
	


	// S3のレコード長を読み込む
	ReadNum = fread(ptStypeRecord->tS3.szLength, 1, sizeof(ptStypeRecord->tS3.szLength), fp);
	if (ReadNum != sizeof(ptStypeRecord->tS3.szLength))
	{
		return false;
	}
	Length = StrHex2Num((const char*)&ptStypeRecord->tS3.szLength);								// レコード長(16進数) : アドレス(4:8/2) + データ(*x2) + CheckSum(1)

	// ロード・アドレスを読み込む
	ReadNum = fread(ptStypeRecord->tS3.szAddress, 1, sizeof(ptStypeRecord->tS3.szAddress), fp);
	if (ReadNum != sizeof(ptStypeRecord->tS3.szAddress))
	{
		return false;
	}
	ptFlashInfo->Address = 0x00000000;
	for (unsigned char i = 0; i < 8; i++)
	{
		ptFlashInfo->Address += atohex(ptStypeRecord->tS3.szAddress[i]);
		if (i < 7)
		{
			ptFlashInfo->Address = ptFlashInfo->Address << 4;
		}
	}

	// コードを読み込む
	ptFlashInfo->DataSize = Length - (4 + 1);
	ReadNum = fread(ptStypeRecord->tS3.szCode, 1, (ptFlashInfo->DataSize * 2), fp);
	if (ReadNum != (ptFlashInfo->DataSize * 2))
	{
		return false;
	}
	for (unsigned char i = 0; i < ptFlashInfo->DataSize; i++)
	{
		ptFlashInfo->Data[i] = StrHex2Num((const char*)&ptStypeRecord->tS3.szCode[(i * 2)]);
	}

	// チェクサム・ニューラインを読み込む
	ReadNum = fread(ptStypeRecord->tS3.szChecksum, 1, (CHECK_SUM_SIZE + NEW_LINE_SIZE), fp);
	if (ReadNum != (CHECK_SUM_SIZE + NEW_LINE_SIZE))
	{
		return false;
	}

	// ニュー・ラインチェック
	if ((ptStypeRecord->tS3.szNewLine[0] != 0x0D) || (ptStypeRecord->tS3.szNewLine[1] != 0x0A))
	{
		return false;
	}

	// チェクサム(レコード長 + アドレス + データ)
	bRet = CheckSum((const char*)&ptStypeRecord->tS3.szLength,
		(sizeof(ptStypeRecord->tS3.szLength) + sizeof(ptStypeRecord->tS3.szAddress) + (ptFlashInfo->DataSize * 2)),
		StrHex2Num((const char*)&ptStypeRecord->tS3.szChecksum));
	if (bRet == false)
	{
		return false;
	}

#if 1
	printf("[%08X] : ", ptFlashInfo->Address);
	for (int j = 0; j < ptFlashInfo->DataSize; j++)
	{
		printf("0x%02X ", ptFlashInfo->Data[j]);
	}
	printf("\n");
#endif

	return true;
}


//-----------------------------------------------------------------------------
// S7レコード解析
//-----------------------------------------------------------------------------
bool Analyze_S7(FILE* fp, S_TYPE_RECORD_TABLE* ptStypeRecord)
{
	size_t ReadNum = 0;
	bool bRet = false;
	unsigned char Length = 0;
	unsigned int EntryPointAddress = 0x00000000;


	// S7のレコード長を読み込む
	ReadNum = fread(ptStypeRecord->tS7.szLength, 1, sizeof(ptStypeRecord->tS7.szLength), fp);
	if (ReadNum != sizeof(ptStypeRecord->tS7.szLength))
	{
		return false;
	}
	Length = StrHex2Num((const char*)&ptStypeRecord->tS7.szLength);								// レコード長(16進数) : エントリポイントアドレス(4:8/2) + CheckSum(1)

	// エントリポイントアドレスを読み込む
	ReadNum = fread(ptStypeRecord->tS7.szEntryPointAddress, 1, sizeof(ptStypeRecord->tS7.szEntryPointAddress), fp);
	if (ReadNum != sizeof(ptStypeRecord->tS7.szEntryPointAddress))
	{
		return false;
	}
	for (unsigned char i = 0; i < sizeof(ptStypeRecord->tS7.szEntryPointAddress); i++)
	{
		EntryPointAddress += atohex(ptStypeRecord->tS7.szEntryPointAddress[i]);
		if (i < 7)
		{
			EntryPointAddress = EntryPointAddress << 4;
		}
	}

	// チェクサム・ニューラインを読み込む
	ReadNum = fread(ptStypeRecord->tS7.szChecksum, 1, (CHECK_SUM_SIZE + NEW_LINE_SIZE), fp);
	if (ReadNum != (CHECK_SUM_SIZE + NEW_LINE_SIZE))
	{
		return false;
	}

	// ニュー・ラインチェック
	if ((ptStypeRecord->tS7.szNewLine[0] != 0x0D) || (ptStypeRecord->tS7.szNewLine[1] != 0x0A))
	{
		return false;
	}

	// チェクサム(レコード長 + エントリポイントアドレス)
	bRet = CheckSum((const char*)&ptStypeRecord->tS7.szLength,
		(sizeof(ptStypeRecord->tS7.szLength) + sizeof(ptStypeRecord->tS7.szEntryPointAddress)),
		StrHex2Num((const char*)&ptStypeRecord->tS7.szChecksum));
	if (bRet == false)
	{
		return false;
	}

	printf("*** Entry Point Address : %08p ***\n", EntryPointAddress);

	return true;
}
		




void DebugDump(CF_WRITE_INFO_TABLE* ptCfWriteInfo)
{
	printf("---[CF_WRITE_INFO]--------------------------------------------------------\n");


	for (unsigned int j = 0; j < (CF_WRITE_MIN_SIZE / 16); j++)
	{
		printf("[%08p] : ", (ptCfWriteInfo->StartAddress + (16 * j)));
		for (unsigned int i = 0; i < 16; i++)
		{
			printf("%02X ", ptCfWriteInfo->Data[(16 * j) + i]);
		}
		printf("\n");
	}
	printf("--------------------------------------------------------------------------\n\n");
}



//-----------------------------------------------------------------------------
// Stypeレコード読み込み
//-----------------------------------------------------------------------------
static unsigned int Read_StypeRecord(FILE* fp, S_TYPE_RECORD_TABLE* ptStypeRecord, FLASH_INFO_TABLE* ptFlashInfo)
{
	bool bRet = false;
	size_t ReadNum = 0;


	// 1文字読み込んで、レコードタイプを調べる
	ReadNum = fread(ptStypeRecord->szRecordName, 1, 2, fp);
	if (ReadNum != 2)
	{
		return 1;
	}

	// レコード種別チェック
	if (ptStypeRecord->szRecordName[0] != 'S')
	{
		return 2;
	}

	switch (ptStypeRecord->szRecordName[1]) {
	case '0':
		ptStypeRecord->eRecordKind = RECORD_KIND_S0;
		bRet = Analyze_S0(fp, ptStypeRecord);
		break;
	case '3':
		ptStypeRecord->eRecordKind = RECORD_KIND_S3;
		bRet = Analyze_S3(fp, ptStypeRecord, ptFlashInfo);
		break;
	case '7':
		ptStypeRecord->eRecordKind = RECORD_KIND_S7;
		bRet = Analyze_S7(fp, ptStypeRecord);
		break;
	case '1':
	case '2':
	case '8':
	case '9':
	default:
		return 2;
	}

	if (bRet == false)
	{
		return 2;
	}

	return 0;
}











int main()
{
	FILE* fp = NULL;
	S_TYPE_RECORD_TABLE tStypeRecord;
	size_t ReadNum = 0;
	bool bRet = false;
	CF_WRITE_INFO_TABLE	tCfWriteInfo;
	FLASH_INFO_TABLE	tFlashInfo;
	unsigned int		pos = 0;


	memset(&tCfWriteInfo.Data, CF_CLEAR_NUM, sizeof(tCfWriteInfo.Data));
	tCfWriteInfo.StartAddress = 0xFFE00000;
	tCfWriteInfo.EndAddress = tCfWriteInfo.StartAddress + (CF_WRITE_MIN_SIZE - 1);


	memset(&tFlashInfo, 0x00, sizeof(tFlashInfo));
	memset(&tStypeRecord, 0x00, sizeof(tStypeRecord));
	fp = fopen("C:\\Users\\MIBC\\source\\repos\\MotorolaHexAnalyze\\Debug\\FwUpdate.mot", "rb");
	if (fp == NULL)
	{
		printf("fopen Error.\n");
		return -1;
	}

#if 0
	while (1)
	{
		// 1文字読み込んで、レコードタイプを調べる
		ReadNum = fread(tStypeRecord.szRecordName, 1, 2, fp);
		if (ReadNum != 2)
		{
			break;
		}

		// レコード種別チェック
		if (tStypeRecord.szRecordName[0] != 'S')
		{
			return -1;
		}

		switch (tStypeRecord.szRecordName[1]) {
		case '0':
			tStypeRecord.eRecordKind = RECORD_KIND_S0;
			bRet = Analyze_S0(fp, &tStypeRecord);
			break;
		case '1':
			break;
		case '2':
			break;
		case '3':
			tStypeRecord.eRecordKind = RECORD_KIND_S3;
			bRet = Analyze_S3(fp, &tStypeRecord, &tFlashInfo);
			break;
		case '7':
			tStypeRecord.eRecordKind = RECORD_KIND_S7;
			bRet = Analyze_S7(fp, &tStypeRecord);
			break;
		case '8':
			break;
		case '9':
			break;
		default:
			break;
		}

		if (bRet == false)
		{
			return -1;
		}


		if (tStypeRecord.eRecordKind == RECORD_KIND_S3)
		{
			if ((tCfWriteInfo.StartAddress == 0) && (tCfWriteInfo.EndAddress == 0))
			{
				// CF書込み範囲設定
				tCfWriteInfo.StartAddress = tFlashInfo.Address;
				tCfWriteInfo.EndAddress = tCfWriteInfo.StartAddress + (CF_WRITE_MIN_SIZE - 1);
				memset(tCfWriteInfo.Data, 0x00, sizeof(tCfWriteInfo.Data));
			}

			// CF書込み範囲確認
			if (tCfWriteInfo.StartAddress > tFlashInfo.Address)
			{
				// 仕様上、あり得ないエラー
				return -1;
			}

			for (unsigned int i = 0; i < tFlashInfo.DataSize; i++)
			{
				if (tCfWriteInfo.EndAddress >= (tFlashInfo.Address + i))
				{
					pos = (tFlashInfo.Address + i) - tCfWriteInfo.StartAddress;
					tCfWriteInfo.Data[pos] = tFlashInfo.Data[i];
				}
				else
				{
					// 書込み処理
					DebugDump(&tCfWriteInfo);

					// 前回、書き込みデータがピッタリだった場合
					if (i == 0)
					{
						tCfWriteInfo.StartAddress = tFlashInfo.Address;
						tCfWriteInfo.EndAddress = tCfWriteInfo.StartAddress + (CF_WRITE_MIN_SIZE - 1);
						memset(tCfWriteInfo.Data, 0x00, sizeof(tCfWriteInfo.Data));
						pos = (tFlashInfo.Address + i) - tCfWriteInfo.StartAddress;
						tCfWriteInfo.Data[pos] = tFlashInfo.Data[i];

					}
					// 書込みデータが余っている場合
					else
					{
						tCfWriteInfo.StartAddress = tCfWriteInfo.EndAddress + 1;
						tCfWriteInfo.EndAddress = tCfWriteInfo.StartAddress + (CF_WRITE_MIN_SIZE - 1);
						memset(tCfWriteInfo.Data, 0x00, sizeof(tCfWriteInfo.Data));
						pos = (tFlashInfo.Address + i) - tCfWriteInfo.StartAddress;
						tCfWriteInfo.Data[pos] = tFlashInfo.Data[i];
					}

				}
			}

		}
	}
#else

	while (1)
	{
		unsigned int  uRet = Read_StypeRecord(fp, &tStypeRecord, &tFlashInfo);
		if (uRet != 0)
		{
			return -1;
		}

		if ((tStypeRecord.eRecordKind == RECORD_KIND_S3) || (tStypeRecord.eRecordKind == RECORD_KIND_S7))
		{
			break;
		}
	}


	while (1)
	{
		// CF書込み領域を1Byteずつチェックする
		for (unsigned int i = 0; i < sizeof(tCfWriteInfo.Data); i++)
		{
			pos = tCfWriteInfo.StartAddress + i;

			// 範囲内の場合
			if ((pos >= tFlashInfo.Address) && (pos <= (tFlashInfo.Address + (tFlashInfo.DataSize - 1))))
			{
				tCfWriteInfo.Data[i] = tFlashInfo.Data[(pos - tFlashInfo.Address)];
				tFlashInfo.WriteNum++;

				if (tFlashInfo.WriteNum >= tFlashInfo.DataSize)
				{
					// 新しい書込みデータを読み込む
					unsigned int  uRet = Read_StypeRecord(fp, &tStypeRecord, &tFlashInfo);
					if (uRet != 0)
					{
						break;
					}
				}
			}
			else if (pos > (tFlashInfo.Address + (tFlashInfo.DataSize - 1)))
			{
				// 新しい書込みデータを読み込む
				unsigned int  uRet = Read_StypeRecord(fp, &tStypeRecord, &tFlashInfo);
				if (uRet != 0)
				{
					break;
				}
			}
		}
		// 書込み処理 
		DebugDump(&tCfWriteInfo);
		memset(&tCfWriteInfo.Data, CF_CLEAR_NUM, sizeof(tCfWriteInfo.Data));
		tCfWriteInfo.StartAddress += CF_WRITE_MIN_SIZE;
		tCfWriteInfo.EndAddress = tCfWriteInfo.StartAddress + (CF_WRITE_MIN_SIZE - 1);
	}
#endif

	if ((tCfWriteInfo.StartAddress != 0) && (tCfWriteInfo.EndAddress != 0))
	{
		// 書込み処理
		DebugDump(&tCfWriteInfo);
	}



	fclose(fp);
}



