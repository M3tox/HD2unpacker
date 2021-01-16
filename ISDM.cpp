// for little additional description view the header file

#include "ISDM.h"


ISDM::ISDM(const std::string& fullPath) {

	// open archive with given file path
	dtaFile.open(fullPath, std::ios::binary);
	if (!dtaFile.good()) {
		errStatus = 3;
		return;
	}

	// read first 4 bytes into buffer
	char regBuffer[4];
	dtaFile.read(regBuffer, 4);

	// check if signature in file is ISD
	if (memcmp(regBuffer, "ISD", 3)) {
		errStatus = 1;
		return;
	}

	// check if signature includes 0 or 1 and set the correct algorithm already.
	if (regBuffer[3] == '0')
		bISD1 = false;
	else if (regBuffer[3] == '1')
		bISD1 = true;
	else {
		dtaFile.close();
		errStatus = 1;
		return; // error, its not ISD0 neither ISD1
	}

	// read archive header
	dtaFile.read(reinterpret_cast<char*>(&ArchiveHeader), sizeof(DTA_ArchiveHeader));

	// Check which file it is:
	// use undecrypted file size as signature
	for (uint8_t sel = 0; sel < KEYCOUNT; sel++) {
		if (fileIndentifiers[sel] == (ArchiveHeader.ahNumOfFiles & 0xFFFFFF00)) {
			keySelector = sel;
			break;
		}
	}

	// little hotfix, not very elegant but it works.
	// the english version of LangEnglish does not match with identifier because it has less files than the german one.
	// if the key is still not found, it will check if it might be this one as well if its not -> Error
	if (keySelector == -1) {
		if (0xA0A0B170 == ArchiveHeader.ahNumOfFiles)
			keySelector = 10;
		else {
			errStatus = 2;
			return;
		}
	}

	Decrypt(&ArchiveHeader, sizeof(DTA_ArchiveHeader));
	// check file size and make sure its bigger than
	// the given table offset position
	std::ios::pos_type lastPos = dtaFile.tellg();
	dtaFile.seekg(0, std::ios::end);
	if (ArchiveHeader.ahFileTableOffset > dtaFile.tellg()) {
		errStatus = 5;
		return;
	}
	dtaFile.seekg(lastPos);

	// jump to table records
	dtaFile.seekg(ArchiveHeader.ahFileTableOffset);

	// reserve enough space on heap and emplace all table records
	dtaFileRecords = new DTA_FileTableRec[ArchiveHeader.ahNumOfFiles];
	dtaFile.read((char*)dtaFileRecords, sizeof(DTA_FileTableRec)*ArchiveHeader.ahNumOfFiles);
	Decrypt(dtaFileRecords, sizeof(DTA_FileTableRec)*ArchiveHeader.ahNumOfFiles);

	//at last, save the file path for later reference:
	filePath.assign(fullPath);

	// the .dta is now mounted and ready to read any file from
	return;
}

ISDM::~ISDM() {
	dtaFile.close();
	delete[] dtaFileRecords;
}

uint16_t ISDM::CreateFileContent(uint32_t FileID, DataBuffer* fileBuffer) {

	dtaFile.clear();
	dtaFile.seekg(dtaFileRecords[FileID].HeaderOffset);

	if (bISD1) {
		// IF archive is ISD1 allocate special ISD1 archive header on stack
		// read it and decrypt it
		DTA_FileHeaderISD1 FileHeader;
		dtaFile.read((char*)&FileHeader, sizeof(FileHeader));
		Decrypt(&FileHeader, sizeof(FileHeader));
		
		// read and decrypt the file name to extract from header
		fileBuffer->FileName.resize(FileHeader.FullFileNameLength);
		dtaFile.read((char*)fileBuffer->FileName.data(), FileHeader.FullFileNameLength);
		Decrypt((void*)fileBuffer->FileName.data(), FileHeader.FullFileNameLength);

		// make sure enough space is allocated in file content buffer
		fileBuffer->resize(FileHeader.fileSize);

		// check if block is encrypted or not (they should be always)
		bool BlockEncryption = FileHeader.flags[0] & 0x80;

		// in ISD1 the blocks are NOT splitten with the block data in between.
		// here the data has to be readen before the block can be read
		// Now all compressed blocks can be read all at once
		// and go though decompression function
		// read all data and decrypt it at once
		DTA_BlockHeaderISD1 Blockheader(FileHeader.compBlocks);
		dtaFile.read(reinterpret_cast<char*>(Blockheader.BlockSizes), sizeof(*Blockheader.BlockSizes)*FileHeader.compBlocks);
		dtaFile.read(reinterpret_cast<char*>(Blockheader.BlockTypes), FileHeader.compBlocks);
		Decrypt(Blockheader.BlockTypes, FileHeader.compBlocks);

		// copy pointer from file buffer
		uint8_t* bufferPtr = fileBuffer->GetContent();

		// iterate though all given blocks
		for (uint32_t i = 0; i < FileHeader.compBlocks; i++) {

			// check block information for each one
			uint32_t SizeOfBlock = Blockheader.BlockSizes[i] & 0xFFFF;

			blockBuffer.resize(SizeOfBlock);
			dtaFile.read(reinterpret_cast<char*>(blockBuffer.GetContent()), SizeOfBlock);

			if (BlockEncryption)
				Decrypt(blockBuffer.GetContent(), SizeOfBlock);
			

			// check which decompression methode has to be used
			switch (Blockheader.BlockTypes[i]) {
			case BLOCK_UNCOMPRESSED:

				memcpy(bufferPtr, blockBuffer.GetContent(), SizeOfBlock);
				bufferPtr += SizeOfBlock;
				break;

			case BLOCK_LZSS_RLE:

				bufferPtr = decompressLZSS(bufferPtr, blockBuffer.GetContent(), SizeOfBlock);
				break;
			default: 
				// if its not 0 or 1 we expect it to be a wave file format and use DPCM
				bufferPtr = decompressDPCM(&WAV_DELTAS[128 * ((Blockheader.BlockTypes[i] / 4) - 2)], bufferPtr, blockBuffer.GetContent(), SizeOfBlock);
				break;
			}
		}
	}
	else {
		// read file header:
		DTA_FileHeader FileHeader;
		dtaFile.read((char*)&FileHeader, sizeof(DTA_FileHeader));
		Decrypt(&FileHeader, sizeof(DTA_FileHeader));

		fileBuffer->FileName.resize(FileHeader.FullFileNameLength);
		dtaFile.read((char*)fileBuffer->FileName.data(), FileHeader.FullFileNameLength);
		Decrypt((void*)fileBuffer->FileName.data(), FileHeader.FullFileNameLength);


		fileBuffer->resize(FileHeader.fileSize);

		uint8_t* bufferPtr = fileBuffer->GetContent();

		// check if block is encrypted or not
		bool BlockEncryption = FileHeader.flags[0] & 0x80;

		for (uint32_t i = 0; i < FileHeader.compBlocks; i++) {

			// unlike in ISD1, in ISD0 the block information
			// has to be taken between each block:

			uint32_t SizeOfBlock;
			dtaFile.read((char*)&SizeOfBlock, sizeof(SizeOfBlock));
			SizeOfBlock = SizeOfBlock & 0xFFFF;

			blockBuffer.resize(SizeOfBlock);		// resize block buffer

			dtaFile.read(reinterpret_cast<char*>(blockBuffer.GetContent()), SizeOfBlock);

			if (BlockEncryption)
				Decrypt(blockBuffer.GetContent(), SizeOfBlock);

			// first element of each block contains the block type
			uint8_t blockType = *(blockBuffer.GetContent());


			switch (blockType) {
			case BLOCK_UNCOMPRESSED:
				// just copy to file buffer if no compression is given
				memcpy(bufferPtr, blockBuffer.GetContent()+1, SizeOfBlock - 1);
				bufferPtr += SizeOfBlock-1;
				break;

			case BLOCK_LZSS_RLE:
				bufferPtr = decompressLZSS(bufferPtr, blockBuffer.GetContent() + 1, SizeOfBlock - 1);
				break;
			default: 
				// expect wave files
				bufferPtr = decompressDPCM(&WAV_DELTAS[128 * ((blockType / 4) - 2)], bufferPtr, blockBuffer.GetContent() + 1, SizeOfBlock - 1);
				break;
			}
		}
	}

	// in case an additional wave header was used, put it to false here
	// so program knows in next file that its not written yet
	// in first iteration
	wavHeaderSet = false;

	return 0;
}


void ISDM::Decrypt(void* pData, uint32_t length) {
	// I found this function here and like it: https://hd2.fandom.com/wiki/DTA
	// It is simple and short. I only did little modifications to it.

	uint64_t key = keys[keySelector];
	// divide given bytes to ecrypt by eight
	// for those, 64bit key can decrypt directly
	uint32_t nLongLong = length / 8;

	// get leftovers with modulo, those will be decrypted in the end
	uint32_t nBytes = length % 8;

	// decrypt everything which within 8byte range
	uint64_t* pLongLong = reinterpret_cast<uint64_t*>(pData);
	for (uint32_t i = 0; i < nLongLong; ++i)
		pLongLong[i] = ~((~pLongLong[i]) ^ key);

	// decrypt potencially leferovers, if there are
	if (nBytes > 0) {
		uint8_t* pByte = reinterpret_cast<uint8_t*>(&pLongLong[nLongLong]);
		uint8_t* pKey = reinterpret_cast<uint8_t*>(&key);
		for (uint32_t i = 0; i < nBytes; ++i)
			pByte[i] = ~((~pByte[i]) ^ pKey[i]);
	}
}

bool ISDM::WriteToFile(void* fileContent, uint32_t sizeBytes, std::string& FileName_str, std::string UserPath = "") {

	std::string::size_type dir_pos{ 0 };

	do {

		if ((dir_pos = FileName_str.find('\\', dir_pos + 1)) != std::string::npos) {
			// yes it is
			std::string directoyPath(UserPath+ FileName_str.substr(0, dir_pos));
			CreateDirectoryA(directoyPath.c_str(), NULL);
		}
		else {
			// it is not, write file directly
			UserPath.append(FileName_str);

			// we are in windows API anyway, so I commented the old code from std library out
			// and use the functions provided by windows
			//std::ofstream extractedFile(UserPath, std::ios::binary);
			//extractedFile.write((char*)fileContent, sizeBytes);
			//extractedFile.close();
			
			DWORD written;
			HANDLE hFile = CreateFileA(UserPath.c_str(), GENERIC_ALL, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

			if (hFile == INVALID_HANDLE_VALUE) {
				return false;
			}

			WriteFile(hFile, fileContent, (DWORD)sizeBytes, &written, NULL);
			CloseHandle(hFile);
			
		}

	} while (dir_pos != std::string::npos);


	return true;
}


uint8_t* ISDM::decompressLZSS(uint8_t* dest, const uint8_t* BlockSource, uint32_t blockSize) {
	// based on HDmaster's research and source but without using vectors
	// instead, it is writing directly into the file buffer
	// I also optimized it a bit by using memset and less "for loops"

	uint32_t BlockPosition{ 0 };

	while (BlockPosition < blockSize) {

		// get first two bytes, 16bit LZSS group
		uint16_t value = (BlockSource[BlockPosition] << 8) | (BlockSource[BlockPosition + 1]);
		BlockPosition += 2;

		// just copy the text if LZSS group is 0
		if (!value) {

			uint8_t segment = min(blockSize - BlockPosition, (uint8_t)16);
			//memcpy(dest+ decompressedPos, BlockSource+BlockPosition, segment);
			memcpy(dest, BlockSource + BlockPosition, segment);
			BlockPosition += segment;
			dest += segment;
			//decompressedPos += segment;
		}
		else {

			// check bit for bit, reading from left
			for (uint32_t i = 0; i < 16 && BlockPosition < blockSize; ++i, value <<= 1) {

				if (value & 0x8000) {
					// bit set, copy sequence from already decompressed sequence
					uint32_t offset = (BlockSource[BlockPosition] << 4) | (BlockSource[BlockPosition + 1] >> 4);
					uint32_t length = BlockSource[BlockPosition + 1] & 0x0f;

					// offset points back to already decompressed bytes, n defines the sequence length

					if (!offset)
					{
						// get number of elements to copy
						length = ((length << 8) | (BlockSource[BlockPosition + 2])) + 16;
						//memcpy(dest, BlockSource + BlockPosition + 3, length);
						memset(dest, BlockSource[BlockPosition + 3], length);
						BlockPosition += 4;
						dest += length;
					}
					else
					{
						length += 3;

						if (length > offset)
						{
							while (length--)
								*dest++ = *(dest - offset);

						}
						else
						{
							memcpy(dest, dest - offset, length);
							dest += length;
						}

						BlockPosition += 2;
					}
				}
				else {
					// bit not set, just copy to destionation buffer
					*dest++ = BlockSource[BlockPosition++];
				}
			}
		}
	}

	// return position to know the decompressed block size
	return dest;
}


// decompression DPCM for wave files
uint8_t* ISDM::decompressDPCM(const uint16_t* delta, uint8_t* dest, const uint8_t* BlockSource, uint32_t blockSize) {

	uint32_t pos{ 0 };

	if (!wavHeaderSet) {

		memcpy((char *)&wavHeader, BlockSource, sizeof(wavHeader));
		pos += sizeof(wavHeader);
		
		memcpy(dest, &wavHeader, sizeof(wavHeader));
		dest += sizeof(wavHeader);
		wavHeaderSet = true;
	}

	if (wavHeader.wChannels == 1)
	{
		*dest++ = BlockSource[pos];
		*dest++ = BlockSource[pos+1];

		uint16_t value = *((uint16_t *) &(BlockSource[pos++]));
		pos++;

		while (pos < blockSize)
		{
			int32_t sign = (BlockSource[pos] & 0x80) == 0 ? 1.0 : -1.0;
			value += sign * delta[BlockSource[pos] & 0x7f];
			*dest++ = (value & 0xff);
			*dest++ = (value >> 8);
			pos++;
		}
	}
	else
	{
		// it seems all wave files use only one channel.
		// if not, if there are stereo files for example, implement the code here.
	}


	return dest;
}


void ISDM::WriteSingleFile(uint32_t fileID, const std::string& savePath) {
	if (errStatus)
		return;

	DataBuffer fBuffer;
	
	CreateFileContent(fileID, &fBuffer);
	void* testPtr = fBuffer.GetContent();
	uint32_t test_val = fBuffer.GetSize();
	WriteToFile(fBuffer.GetContent(), fBuffer.GetSize(), fBuffer.FileName, savePath);
	
	return;
}

// write all files directly
void ISDM::WriteAllFiles() {

	if (errStatus)
		return;

	DataBuffer fBuffer;;

	for (uint32_t exFile = 0; exFile < ArchiveHeader.ahNumOfFiles; exFile++) {
		CreateFileContent(exFile, &fBuffer);
		WriteToFile(fBuffer.GetContent(), fBuffer.GetSize(), fBuffer.FileName);
	}


	return;
}

// write all files and get the status by given integer
void ISDM::WriteAllFiles(uint32_t& currentFile) {

	if (errStatus)
		return;

	DataBuffer fBuffer;

	for (; currentFile < ArchiveHeader.ahNumOfFiles; currentFile++) {
		CreateFileContent(currentFile, &fBuffer);
		WriteToFile(fBuffer.GetContent(), fBuffer.GetSize(), fBuffer.FileName);
	}

	return;
}

// write all files but update the progress bar from a windows API
void ISDM::WriteAllFiles(
	bool& UserCancel, 
	uint32_t& counter,
	HWND progressBarHandle, 
	HWND ProgressTitle, 
	const std::string& savePath) {

	if (errStatus)
		return;

	DataBuffer fBuffer;
	//int32_t counter{ 0 };
	//std::ofstream buffFile("TestFileBuffer.txt");

	//for (uint32_t exFile = 0; exFile < dtaFileRecords.size(); exFile++) {
	while (counter < ArchiveHeader.ahNumOfFiles){

		if (UserCancel)
			return;

		CreateFileContent(counter, &fBuffer);
		WriteToFile(fBuffer.GetContent(), fBuffer.GetSize(), fBuffer.FileName, savePath);
		

		SetWindowTextA(ProgressTitle, fBuffer.FileName.c_str());
		SendMessage(progressBarHandle, PBM_STEPIT, 0, 0);
		counter++;
		//buffFile << "File " << exFile+1 << ": " << fBuffer.FileName  << ", Buffer: " << fBuffer.MaxBufferSize << '\n';
	}
	//buffFile.close();

	std::string finalMsg;
	finalMsg.assign("File extraction succesful!\n");
	finalMsg.append(std::to_string(counter));
	finalMsg.append(" files extracted.");
	MessageBoxA(GetParent(ProgressTitle), finalMsg.c_str(), "Info", MB_OK);

	return;
}

uint32_t ISDM::GetFileCount() {
	return ArchiveHeader.ahNumOfFiles;
}

////////////////////////////////////////////////////////////
/////   ADDITIONAL FUNCTIONS FOR DEBUGGING/OBSERVING   /////
////////////////////////////////////////////////////////////

// Function is mostly for debugging purpose, writes Table content to a file if needed
void ISDM::WriteTableRecords(const char* fileName) {

	if (errStatus) {
		return;
	}

	std::ofstream TableRecordsFile(fileName);

	TableRecordsFile << "Archive header Information:\n";
	TableRecordsFile << "Unknown value:                 " << ArchiveHeader.ahExtra << '\n';
	//for (int j = 0; j < 4; j++)
	//void* test = &ArchiveHeader.ahExtra;
	//	printf("%02X", ArchiveHeader.ahExtra);
	TableRecordsFile << "Table Record size in bytes:    " << ArchiveHeader.ahFileTableSize << '\n';
	TableRecordsFile << "Table Record at file position: " << ArchiveHeader.ahFileTableOffset << '\n';
	TableRecordsFile << "Amount of files:               " << ArchiveHeader.ahNumOfFiles << "\n\n";
	TableRecordsFile << "Table records:\n\n";

	for (uint32_t i = 0; i < ArchiveHeader.ahNumOfFiles; i++) {
		TableRecordsFile << "File index:         " << i << '\n';
		TableRecordsFile << "Checksum:           " << dtaFileRecords[i].checkSum << '\n';
		TableRecordsFile << "File name length:   " << dtaFileRecords[i].fileNameLength << '\n';
		TableRecordsFile << "Offset file header: " << dtaFileRecords[i].HeaderOffset << '\n';
		TableRecordsFile << "Offset file data:   " << dtaFileRecords[i].DataOffset << '\n';
		TableRecordsFile << "file name hint:     ";
		TableRecordsFile.write(dtaFileRecords[i].filenameHint, 16);
		TableRecordsFile.write("\0\n", 2);
		TableRecordsFile << "\n-------------------------------------------------------------------\n\n";
	}

	TableRecordsFile.close();
};

// write all file headers to specified file
void ISDM::WriteFileHeaders(const char* fileName) {

	if (errStatus) {
		return;
	}

	std::ofstream HeadersFile(fileName);

	HeadersFile << "Headers written:            " << ArchiveHeader.ahNumOfFiles << "\n\n";


	for (uint32_t i = 0; i < ArchiveHeader.ahNumOfFiles; i++) {

		char FileNameBuffer[255];

		dtaFile.clear();
		dtaFile.seekg(dtaFileRecords[i].HeaderOffset);

		if (bISD1) {
			DTA_FileHeaderISD1 FileHeader;

			dtaFile.read((char*)&FileHeader, sizeof(DTA_FileHeaderISD1));
			Decrypt(&FileHeader, sizeof(DTA_FileHeaderISD1));

			dtaFile.read(FileNameBuffer, FileHeader.FullFileNameLength);
			Decrypt(FileNameBuffer, FileHeader.FullFileNameLength);
			FileNameBuffer[FileHeader.FullFileNameLength] = '\0';

			HeadersFile << "File index:              " << i << '\n';
			HeadersFile << "Unknown value 1:         " << FileHeader.extra1 << '\n';
			HeadersFile << "Unknown value 2:         " << FileHeader.extra2 << '\n';
			HeadersFile << "Time stamp as FILETIME:  " << FileHeader.timeStamp << '\n';
			HeadersFile << "File size in bytes:      " << FileHeader.fileSize << '\n';
			HeadersFile << "Amount of File Blocks:   " << FileHeader.compBlocks << '\n';
			HeadersFile << "Unknown value 3:         " << FileHeader.extra3 << '\n';
			HeadersFile << "File name length:        " << (int)FileHeader.FullFileNameLength << '\n';
			HeadersFile << "Full file Name:          ";
			HeadersFile << FileNameBuffer;
			HeadersFile << '\n';
			HeadersFile << "\n-------------------------------------------------------------------\n\n";

		}
		else {
			DTA_FileHeader FileHeader;

			dtaFile.read((char*)&FileHeader, sizeof(DTA_FileHeader));
			Decrypt(&FileHeader, sizeof(DTA_FileHeader));

			dtaFile.read(FileNameBuffer, FileHeader.FullFileNameLength);
			Decrypt(FileNameBuffer, FileHeader.FullFileNameLength);
			FileNameBuffer[FileHeader.FullFileNameLength] = '\0';

			HeadersFile << "File index:              " << i << '\n';
			HeadersFile << "Unknown value 1:         " << FileHeader.extra1 << '\n';
			HeadersFile << "Unknown value 2:         " << FileHeader.extra2 << '\n';
			HeadersFile << "Time stamp as FILETIME:  " << FileHeader.timeStamp << '\n';
			HeadersFile << "File size in bytes:      " << FileHeader.fileSize << '\n';
			HeadersFile << "Amount of File Blocks:   " << FileHeader.compBlocks << '\n';
			HeadersFile << "File name length:        " << (int)FileHeader.FullFileNameLength << '\n';
			HeadersFile << "Full file Name:          ";
			HeadersFile << FileNameBuffer;
			HeadersFile << '\n';
			HeadersFile << "\n-------------------------------------------------------------------\n\n";
		}
	}

	HeadersFile.close();
}

void ISDM::WriteFileNames(const char* outputName) {
	char FileNameBuffer[255];

	std::ofstream HeadersFile(outputName);

	HeadersFile << "files included: " << ArchiveHeader.ahNumOfFiles << "\n\n";

	for (uint32_t i = 0; i < ArchiveHeader.ahNumOfFiles; i++) {
		dtaFile.clear();
		dtaFile.seekg(dtaFileRecords[i].HeaderOffset);

		if (bISD1) {
			dtaFile.seekg(sizeof(DTA_FileHeaderISD1), std::ios::cur );

			dtaFile.read(FileNameBuffer, dtaFileRecords[i].fileNameLength);
			Decrypt(FileNameBuffer, dtaFileRecords[i].fileNameLength);
			FileNameBuffer[dtaFileRecords[i].fileNameLength] = '\0';
		}
		else {
			dtaFile.seekg(sizeof(DTA_FileHeader), std::ios::cur);

			dtaFile.read(FileNameBuffer, dtaFileRecords[i].fileNameLength);
			Decrypt(FileNameBuffer, dtaFileRecords[i].fileNameLength);
			FileNameBuffer[dtaFileRecords[i].fileNameLength] = '\0';
		}

		HeadersFile << FileNameBuffer << '\n';
	}

	HeadersFile.close();
}

void ISDM::CloseFile() {
	errStatus = 4;
	dtaFile.close();
}

const char* ISDM::GetFileName() {
	return origFileNames[keySelector];
}

const int32_t ISDM::GetFileIndexByHint(const char* NameHint, uint8_t strSize) {

	if (strSize > 16) {
		NameHint += (strSize - 16);
		strSize = 16;
	}
		

	for (int32_t index = 0; index < ArchiveHeader.ahNumOfFiles; index++) {
			void* test = dtaFileRecords[index].filenameHint;
		if (dtaFileRecords[index].fileNameLength >= strSize) {
			void* test = dtaFileRecords[index].filenameHint;

			if (dtaFileRecords[index].fileNameLength > 16) {
				if (!memcmp(dtaFileRecords[index].filenameHint + 16 - strSize, NameHint, strSize))
					return index;
			}
			else {
				if (!memcmp(dtaFileRecords[index].filenameHint + dtaFileRecords[index].fileNameLength - strSize, NameHint, strSize))
					return index;
			}
		}
	}
	return -1;
}

ISDM::DataBuffer* ISDM::GetSingleFile(uint32_t fileIndex) {

	DataBuffer* Buffer = new DataBuffer;

	CreateFileContent(fileIndex, Buffer);

	return Buffer;
}

uint16_t ISDM::GetLastError() {
	return errStatus;
}

uint16_t ISDM::GetLastError(char* err) {

	switch (errStatus) {
	case 0:
		err = nullptr;	// No errors
		break;
	case 1:
		memcpy(err, "Wrong format. File must be in either ISD0 or ISD1!", 51);
		break;
	case 2:
		memcpy(err, "File could not be identified! decryption key not defined.", 58);
		break;
	case 3:
		memcpy(err, "No File opened! Constructor was not able to read file!", 55);
		break;
	case 4:
		memcpy(err, ".dta File was already closed! Create a new object.", 51);
		break;
	case 5:
		memcpy(err, "File decryption failed! Encrypted data not reasonable.", 55);
		break;
	default:
		memcpy(err, "Unknown Error occured!", 23);
		break;
	}

	return errStatus;
}
