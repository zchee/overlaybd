/*
   Copyright The Overlaybd Authors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#pragma once
#include <inttypes.h>

namespace FileSystem
{
    class IFile;
    class IFileSystem;

    // create a view of sub tree of an underlay fs
    IFileSystem* new_subfs(IFileSystem* underlayfs, const char* base_path, bool ownership);

    // create a view of part of an underlay file
    // only for pread/v and pwrite/v
    IFileSystem* new_subfile(IFile* underlay_file, uint64_t offset, uint64_t length, bool ownership);
}
