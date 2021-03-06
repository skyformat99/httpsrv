//
// This file is part of httpsrv
// Copyright (c) Antonino Calderone (antonino.calderone@gmail.com)
// All rights reserved.
// Licensed under the MIT License.
// See COPYING file in the project root for full license information.
//

/* -------------------------------------------------------------------------- */

#include "FileRepository.h"
#include "ZipArchive.h"
#include "StrUtils.h"
#include "config.h"

#include <fstream>
#include <chrono>


/* -------------------------------------------------------------------------- */

bool FileRepository::init()
{
   std::string repositoryPath;

   // Resolve any homedir prefix
   std::string resPath;

   if (!_path.empty())
   {
      size_t prefixSize = _path.size() > 1 ? 2 : 1;
      if ((prefixSize > 1 && _path.substr(0, 2) == "~/") || _path == "~")
      {
         resPath = FileUtils::getHomeDir();
         resPath += "/";
         resPath += _path.substr(prefixSize, _path.size() - prefixSize);
      }
      else
      {
         resPath = _path;
      }
   }

   if (!FileUtils::touchDir(resPath, repositoryPath))
      return false;

   try
   {
      fs::path cPath(repositoryPath);
      _path = fs::canonical(cPath).string();
   }
   catch (...)
   {
      return false;
   }

   return true;
}

/* -------------------------------------------------------------------------- */

bool FileRepository::createTimeOrderedFilesList(TimeOrderedFileList &list)
{
   fs::path dirPath(getPath());
   fs::directory_iterator endIt;

   if (fs::exists(dirPath) && fs::is_directory(dirPath))
   {
      list.clear();
      for (fs::directory_iterator it(dirPath); it != endIt; ++it)
      {
         if (fs::is_regular_file(it->status())) {
#if defined ( _WIN32 )
            struct _stat64 fileInfo;
            if (_wstati64(it->path().wstring().c_str(), &fileInfo) != 0)
               return false;

            std::time_t key = std::time_t( fileInfo.st_mtime );
#else
            auto fsTime = fs::last_write_time(it->path());
            auto key = fsTime.time_since_epoch().count();
#endif
            list.insert({ key, *it });
         }
      }
      return true;
   }

   return false;
}

/* -------------------------------------------------------------------------- */

bool FileRepository::createMruFilesList(std::list<std::string> &mrufiles)
{
   TimeOrderedFileList timeOrderedFileList;

   if (!createTimeOrderedFilesList(timeOrderedFileList))
      return false;

   int fileCnt = 0;

   for (auto it = timeOrderedFileList.rbegin();
        it != timeOrderedFileList.rend() && fileCnt < _mrufilesN;
        ++it,
        ++fileCnt)
   {
      mrufiles.push_back(it->second.filename().string());
   }

   return true;
}

/* -------------------------------------------------------------------------- */

bool FileRepository::createJsonMruFilesList(std::string &json)
{
   TimeOrderedFileList timeOrderedFileList;

   if (!createTimeOrderedFilesList(timeOrderedFileList))
      return false;

   json = "[\n";

   int fileCnt = 0;

   for (auto it = timeOrderedFileList.rbegin();
        it != timeOrderedFileList.rend() && fileCnt < _mrufilesN;
        ++it,
             ++fileCnt)
   {
      std::string jsonEntry;
      auto fName = it->second.filename().string();
      auto id = FileUtils::hashCode(fName);
      if (FilenameMap::jsonStat(it->second.string(), fName, id, jsonEntry, "  ", ",\n"))
      {
         json += jsonEntry;
      }
   }

   // remove last ",\n" sequence
   if (json.size() > 2)
      json.resize(json.size() - 2);

   json += "\n]\n";
   return true;
}

/* -------------------------------------------------------------------------- */

bool FileRepository::store(
   const std::string& fileName,
   const std::string& fileContent,
   std::string& json)
{
   fs::path filePath(_path);
   filePath /= fileName;

   std::ofstream os(filePath.string(), std::ofstream::binary);

   os.write(fileContent.data(), fileContent.size());

   if (!os.fail())
   {
      auto id = FileUtils::hashCode(fileName);

      os.close(); // create the file posted by client
      if (!FilenameMap::jsonStat(filePath.string(), fileName, id, json))
      {
         json.clear();
      }
      else
      {
         getFilenameMap().insert(id, fileName);
         return true;
      }
   }

   return false;
}

/* -------------------------------------------------------------------------- */

bool FileRepository::createMruFilesZip(
   std::string& zipFileName,
   FileUtils::DirectoryRipper::Handle& zipCleaner)
{
   fs::path tempDir;
   if (!FileUtils::createTemporaryDir(tempDir))
      return false;

   zipCleaner = std::make_shared<FileUtils::DirectoryRipper>(tempDir);

   std::list<std::string> fileList;
   if (!createMruFilesList(fileList))
      return false;

   tempDir /= MRU_FILES_ZIP_NAME;
   ZipArchive zipArchive(tempDir.string());

   if (!zipArchive.create())
      return false;

   for (const auto& fileName : fileList)
   {
      fs::path src(_path);
      src /= fileName;

      if (!zipArchive.add(src.string(), fileName))
         return false;
   }

   zipArchive.close();
   zipFileName = tempDir.string();

   return true;
}

/* -------------------------------------------------------------------------- */

FileRepository::createFileZipRes FileRepository::createFileZip(
   const std::string id, 
   std::string& zipFileName,
   FileUtils::DirectoryRipper::Handle& zipCleaner)
{
   std::string fileName;

   if (!getFilenameMap().locked_search(id, fileName))
      return createFileZipRes::idNotFound;

   fs::path tempDir;
   if (!FileUtils::createTemporaryDir(tempDir))
      return createFileZipRes::cantCreateTmpDir;

   zipCleaner = std::make_shared<FileUtils::DirectoryRipper>(tempDir);

   tempDir /= fileName + ".zip";
   fs::path src(_path);
   src /= fileName;

   const auto updated = FileUtils::touch(
      src.string(), 
      false /*== do not create if it does not exist*/);

   ZipArchive zipArchive(tempDir.string());
   if (!updated || !zipArchive.create() || !zipArchive.add(src.string(), fileName))
      return createFileZipRes::cantZipFile;

   zipArchive.close();

   zipFileName = tempDir.string();

   return createFileZipRes::success;
}
