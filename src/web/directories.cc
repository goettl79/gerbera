/*MT*
    
    MediaTomb - http://www.mediatomb.cc/
    
    directories.cc - this file is part of MediaTomb.
    
    Copyright (C) 2005 Gena Batyan <bgeradz@mediatomb.cc>,
                       Sergey 'Jin' Bostandzhyan <jin@mediatomb.cc>
    
    Copyright (C) 2006-2010 Gena Batyan <bgeradz@mediatomb.cc>,
                            Sergey 'Jin' Bostandzhyan <jin@mediatomb.cc>,
                            Leonhard Wimmer <leo@mediatomb.cc>
    
    MediaTomb is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2
    as published by the Free Software Foundation.
    
    MediaTomb is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    version 2 along with MediaTomb; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
    
    $Id$
*/

/// \file directories.cc

#include "pages.h" // API

#include <utility>

#include "database/database.h"
#include "util/string_converter.h"
#include "util/tools.h"

web::directories::directories(std::shared_ptr<Config> config, std::shared_ptr<Database> database,
    std::shared_ptr<ContentManager> content, std::shared_ptr<SessionManager> sessionManager)
    : WebRequestHandler(std::move(config), std::move(database), std::move(content), std::move(sessionManager))
{
}

struct dirInfo {
    std::string filename;
    bool hasContent;
};

void web::directories::process()
{
    check_request();

    fs::path path;
    std::string parentID = param("parent_id");
    if (parentID.empty() || parentID == "0")
        path = FS_ROOT_DIRECTORY;
    else
        path = hexDecodeString(parentID);

    auto root = xmlDoc->document_element();

    auto containers = root.append_child("containers");
    xml2JsonHints->setArrayName(containers, "container");
    containers.append_attribute("parent_id") = parentID.c_str();
    if (!param("select_it").empty())
        containers.append_attribute("select_it") = param("select_it").c_str();
    containers.append_attribute("type") = "filesystem";

    // don't bother users with system directorties
    std::vector<fs::path> excludes_fullpath = {
        "/bin",
        "/boot",
        "/dev",
        "/etc",
        "/lib", "/lib32", "/lib64", "/libx32",
        "/proc",
        "/run",
        "/sbin",
        "/sys",
        "/tmp",
        "/usr",
        "/var"
    };
    // don't bother users with special or config directorties
    std::vector<std::string> excludes_dirname = {
        "lost+found",
    };
    bool exclude_config_dirs = true;

    std::error_code ec;
    std::map<std::string, struct dirInfo> filesMap;

    for (const auto& it : fs::directory_iterator(path, ec)) {
        const fs::path& filepath = it.path();

        if (!it.is_directory(ec))
            continue;
        if (std::find(excludes_fullpath.begin(), excludes_fullpath.end(), filepath) != excludes_fullpath.end())
            continue;
        if (std::find(excludes_dirname.begin(), excludes_dirname.end(), filepath.filename()) != excludes_dirname.end()
            || (exclude_config_dirs && startswith(filepath.filename(), ".")))
            continue;

        bool hasContent = false;
        for (auto& subIt : fs::directory_iterator(filepath, ec)) {
            if (!subIt.is_directory(ec) && !isRegularFile(subIt.path(), ec))
                continue;
            hasContent = true;
            break;
        }

        /// \todo replace hexEncode with base64_encode?
        std::string id = hexEncode(filepath.c_str(), filepath.string().length());
        filesMap[id] = { filepath.filename(), hasContent };
    }

    for (const auto& entry : filesMap) {
        auto ce = containers.append_child("container");
        ce.append_attribute("id") = entry.first.c_str();
        ce.append_attribute("child_count") = entry.second.hasContent;

        auto f2i = StringConverter::f2i(config);
        ce.append_attribute("title") = f2i->convert(entry.second.filename).c_str();
    }
}
