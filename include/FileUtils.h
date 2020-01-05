//
// This file is part of httpsrv
// Copyright (c) Antonino Calderone (antonino.calderone@gmail.com)
// All rights reserved.  
// Licensed under the MIT License. 
// See COPYING file in the project root for full license information.
//


/* -------------------------------------------------------------------------- */

#ifndef __FILE_UTILS__H__
#define __FILE_UTILS__H__

#include "StrUtils.h"
#include "IdFileNameCache.h"

#include <map>

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;


/* -------------------------------------------------------------------------- */

namespace FileUtils {

   using TimeOrderedFileList = std::multimap<std::time_t, fs::path>;

   /**
     * Initializes the repository (creates an empty dir if not already present) 
     * to a given path.
     * @return the actual path of repository, or empty string in case of errors
     */
   std::string initRepository(const std::string& path);

   /**
     * Scans the repository saving the content in a time ordered list.
     * Update a given instance of IdFileNameCache related to repository files.
     * @param path of repository
     * @param list is the time ordered list of file found in the given path
     * @param idFileNameCache is the IdFileNameCache instance to be updated
     * @return true if operation successfully completed, false otherwise
     */
   bool scanRepository(
      const std::string& path, 
      TimeOrderedFileList& list, 
      IdFileNameCache& idFileNameCache);

   /**
     * Scans the repository to update a given idFileNameCache
     * @param path of repository
     * @param list is the time ordered list of file found in the given path
     * @param idFileNameCache is the IdFileNameCache instance to be updated
     * @param json is the JSON formatted text matching the cache content
     * @return true if operation successfully completed, false otherwise
     */
   bool refreshIdFilenameCache(
      const std::string& path,
      IdFileNameCache& idFileNameCache,
      std::string& json);

   /**
     * Returns file attributes of fileName.
     *
     * @param fileName String containing the path of existing file
     * @param dateTime Time of last modification of file
     * @param ext File extension or "." if there is no any
     * @return true if operation successfully completed, false otherwise
     */
   bool fileStat(
      const std::string& fileName, 
      std::string& dateTime,
      std::string& ext, 
      size_t& fsize);

   /**
     * Returns file attributes of fileName formatted using a JSON record of
     * id, name, size, timestame, where id is sha256 of fileName,
     * name is the fileName, size is the size in bytes of file,
     * timestamp is a unix access timestamp of file
     *
     * @param filePath String containing complete file path and name
     * @param fileName String containing the name to generate JSON output
     * @param jsonOutput output JSON string
     * @return true if operation successfully completed, false otherwise
     */
   bool jsonStat(
      const std::string& filePath,
      const std::string& fileName,
      const std::string& id,
      std::string& jsonOutput,
      const std::string& beginl="",
      const std::string& endl="\n");

   /**
     * Trivial but portable version of basic touch command
     *
     * @param fileName String containing the path of existing or new file
     * @return true if operation successfully completed, false otherwise
     */
   bool touch(const std::string& fileName);

   /**
     * Gets full path of existing file or directory
     *
     * @param partialPath String containing the path of existing file
     * @param fullPath String containing the full path of existing file
     * @return true if operation successfully completed, false otherwise
     */
   bool getFullPath(const std::string& partialPath, std::string& fullPath);

   /**
     * Return true if pathName exists and it is an accessable directory
     *
     * @param pathName String containing a path of directory
     * @return true if directory found, false otherwise
     */
   bool directoryExists(const std::string& pathName);

   /**
     * Creates a new relative directory relativeDirName if not already existant
     * and retreives the full path name
     *
     * @param relativeDirName String containing a path of directory
     * @param fullPath String that will contain the full path of the directory
     * @return true if directory found, false otherwise
     */
   bool touchDir(const std::string& relativeDirName, std::string& fullPath);

   /**
     * Returns user's home directory
     *
     * @return string containig user home directory path
     */
   std::string getHomeDir();


   /**
     * Create an unique id from a given string
     *
     * @param src Source string
     * @return id
     */
    std::string hashCode(const std::string& src);
};


/* ------------------------------------------------------------------------- */

#endif // !__FILE_UTILS__H__