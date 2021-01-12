# Documentation

If you are a programmer, who wants to do something with H&D2 archives, this document might be interesting for you. 
The ISDM class is used here but it has actually more functions to offer and if my time allows it, I will add more functionalities.
If you have suggestions what could be added, feel free to contact me.

After you have mounted the archive by creating in instance you can
access its public methodes, which allow you to:
- Write table record to text file
- Write all File headers to text file
- write a list of all files this archive includes
- Write all files with 3 overloadings
- Write a single file
- Get single files content
- Get archive file identification name
- Loopup file index by searching with file Hint (16 bytes size max)
- Get amount of files in given archive

If you could use some of that feel free to copy out the class from the uploaded header and CPP file.
Now I will provide some further explanation to its public functions but most of them are self explaining:

# ISDM(const std::string&)
The constructor takes as a string the full file name and will directly start checking if the format is right, 
select the correct decryption key and read the table record already. There is no default constructor, you must specify the file name when instantiating.

# ~ISDM()
The destructor just closes the archive file and frees the heap from the table record.

# void WriteTableRecords(const char*)
If you need to know what the table record provides, you can just pass a file name to this method (.txt format will do) and it will write it.

# void WriteWriteFileHeaders(const char*)
Same as WriteTableRecords but for the file headers the .dta archive has.

# void WriteFileNames(const char*)
Give it a name of a .txt file and it will write you a list of all files the archive includes.

# void WriteAllFiles()
Writes directly all files to the same location where the program is you run the ISDM class with.

# void WriteAllFiles(uint32_t&)
Writes all files but you can give it an unsigned integer. This value will be used to iterate though the writing files, so its like a counter.
If the value you pass is higher than 0, it will start from the number specified, so make sure it is not higher than the archives amount of files.

# void WriteAllFiles(bool&, uint32_t&, HWND, HWND, const std::string&)
If you do not use the windows API, you can ignore this overload. I used this specifically for the HD2unpacker.exe.
Bool&: Is the unpacking progress canceld? It will break out if this is true.
uint32_t&: again a value for the counter, same as in the method explained before.
2x HWND: just two handles to the progress bar element and the status text (which file is being unpacked)
string&: The file path you want to extract to.

# void WriteSingleFile(uint32_t, const std::string&)
If you want to write just a single file, this is the method you are looking for. Give it the file index and the name of the new file and thats it.

# ISDM::FileBuffer* GetSingleFile(uint32_t)
Returns the file buffer from the class containing all file information you might need, as in file name and file content.
The FileBuffer is a class inside the ISDM class but its public, so you can just access it outside to use the buffers methodes.
IMPORTANT: The buffer is allocated on the heap, so dont forget to delete it when you are done processing the file!

# const char* GetFileName()
Returns a pointer to the files identification. Those names are constants. They are independent from the file name and just help to know what archive is mounted.

# uint32_t GetFileCount()
Returns the number of files inside the mounted archive

# uint16_t GetLastError()
If your methodes do not work, you can use this. It will return an error number. If everything is fine it returns 0.

# uint16_t GetLastError(char*)
Same method as before but here you can actually pass in an array of chars. An error message will be written into the specified buffer.
Make sure buffer size is atleast 60 bytes!

