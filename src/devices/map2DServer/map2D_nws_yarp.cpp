/*
 * Copyright (C) 2006-2020 Istituto Italiano di Tecnologia (IIT)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <sstream>
#include <limits>
#include "Map2D_nws_yarp.h"
#include <yarp/dev/IMap2D.h>
#include <yarp/dev/INavigation2D.h>
#include <yarp/dev/GenericVocabs.h>
#include <yarp/math/Math.h>
#include <yarp/os/Log.h>
#include <yarp/os/LogComponent.h>
#include <yarp/os/LogStream.h>
#include <mutex>
#include <cstdlib>
#include <fstream>
#include <yarp/os/Publisher.h>
#include <yarp/os/Subscriber.h>
#include <yarp/os/Node.h>
#include <yarp/rosmsg/TickDuration.h>
#include <yarp/rosmsg/TickTime.h>

using namespace yarp::sig;
using namespace yarp::dev;
using namespace yarp::dev::Nav2D;
using namespace yarp::os;
using namespace std;

namespace {
YARP_LOG_COMPONENT(MAP2DSERVER, "yarp.device.map2DServer")
}

/**
  * Map2D_nws_yarp
  */

Map2D_nws_yarp::Map2D_nws_yarp()
{
}

Map2D_nws_yarp::~Map2D_nws_yarp() = default;

void Map2D_nws_yarp::parse_vocab_command(yarp::os::Bottle& in, yarp::os::Bottle& out)
{
    int code = in.get(0).asVocab();
//     bool ret = false;
    if (code == VOCAB_IMAP)
    {
        int cmd = in.get(1).asVocab();
        if (cmd == VOCAB_IMAP_SET_MAP)
        {
            MapGrid2D the_map;
            Value& b = in.get(2);
            if (Property::copyPortable(b, the_map))
            {
                string map_name = the_map.getMapName();
                auto it = m_maps_storage.find(map_name);
                if (it == m_maps_storage.end())
                {
                    //add a new map
                    m_maps_storage[map_name] = the_map;
                    out.clear();
                    out.addVocab(VOCAB_IMAP_OK);
                }
                else
                {
                    //the map already exists
                    m_maps_storage[map_name] = the_map;
                    out.clear();
                    out.addVocab(VOCAB_IMAP_OK);
                }
            }
            else
            {
                out.clear();
                out.addVocab(VOCAB_IMAP_ERROR);
                yCError(MAP2DSERVER) << "Error in copyPortable";
            }
        }
        else if (cmd == VOCAB_IMAP_GET_MAP)
        {
            string name = in.get(2).asString();
            auto it = m_maps_storage.find(name);
            if (it != m_maps_storage.end())
            {
                out.clear();
                out.addVocab(VOCAB_IMAP_OK);
                yarp::os::Bottle& mapbot = out.addList();
                Property::copyPortable(it->second, mapbot);
            }
            else
            {
                out.clear();
                out.addVocab(VOCAB_IMAP_ERROR);
                yCError(MAP2DSERVER) << "Map" << name << "not found";
            }
        }
        else if (cmd == VOCAB_IMAP_GET_NAMES)
        {
            out.clear();
            out.addVocab(VOCAB_IMAP_OK);

            for (auto& it : m_maps_storage)
            {
                out.addString(it.first);
            }
        }
        else if (cmd == VOCAB_IMAP_REMOVE)
        {
            string name = in.get(2).asString();
            size_t rem = m_maps_storage.erase(name);
            if (rem == 0)
            {
                yCError(MAP2DSERVER) << "Map not found";
                out.clear();
                out.addVocab(VOCAB_IMAP_ERROR);
            }
            else
            {
                out.clear();
                out.addVocab(VOCAB_IMAP_OK);
            }
        }
        else if (cmd == VOCAB_IMAP_CLEAR)
        {
            m_maps_storage.clear();
            out.clear();
            out.addVocab(VOCAB_IMAP_OK);
        }
        else if (cmd == VOCAB_IMAP_SAVE_COLLECTION)
        {
            string mapfile = in.get(2).asString();
            if (saveMaps(mapfile))
            {
                out.clear();
                out.addVocab(VOCAB_IMAP_OK);
            }
            else
            {
                yCError(MAP2DSERVER, "Unable to save collection");
                out.clear();
                out.addVocab(VOCAB_IMAP_ERROR);
            }
        }
        else if (cmd == VOCAB_IMAP_LOAD_COLLECTION)
        {
            string mapfile = in.get(2).asString();
            if (loadMaps(mapfile))
            {
                out.clear();
                out.addVocab(VOCAB_IMAP_OK);
            }
            else
            {
                yCError(MAP2DSERVER, "Unable to load collection");
                out.clear();
                out.addVocab(VOCAB_IMAP_ERROR);
            }
        }
        else
        {
            yCError(MAP2DSERVER, "Invalid vocab received in Map2D_nws_yarp");
            out.clear();
            out.addVocab(VOCAB_IMAP_ERROR);
        }
    }
    else if (code == VOCAB_INAVIGATION)
    {
        int cmd = in.get(1).asVocab();
        if (cmd == VOCAB_NAV_GET_LIST_X && in.get(2).asVocab() == VOCAB_NAV_LOCATION)
        {
            std::string info;

            out.addVocab(VOCAB_OK);
            Bottle& l = out.addList();

            std::map<std::string, Map2DLocation>::iterator it;
            for (it = m_locations_storage.begin(); it != m_locations_storage.end(); ++it)
            {
                l.addString(it->first);
            }
            yCInfo(MAP2DSERVER) << "The following locations are currently stored in the server:" << l.toString();
//             ret = true;
        }
        else if (cmd == VOCAB_NAV_GET_LIST_X && in.get(2).asVocab() == VOCAB_NAV_AREA)
        {
            std::string info;

            out.addVocab(VOCAB_OK);
            Bottle& l = out.addList();

            std::map<std::string, Map2DArea>::iterator it;
            for (it = m_areas_storage.begin(); it != m_areas_storage.end(); ++it)
            {
                l.addString(it->first);
            }
            yCInfo(MAP2DSERVER) << "The following areas are currently stored in the server:" << l.toString();
//             ret = true;
        }
        else if (cmd == VOCAB_NAV_GET_LIST_X && in.get(2).asVocab() == VOCAB_NAV_PATH)
        {
            std::string info;

            out.addVocab(VOCAB_OK);
            Bottle& l = out.addList();

            std::map<std::string, Map2DPath>::iterator it;
            for (it = m_paths_storage.begin(); it != m_paths_storage.end(); ++it)
            {
                l.addString(it->first);
            }
            yCInfo(MAP2DSERVER) << "The following paths are currently stored in the server: " << l.toString();
            //             ret = true;
        }
        else if (cmd == VOCAB_NAV_CLEARALL_X && in.get(2).asVocab() == VOCAB_NAV_LOCATION)
        {
            m_locations_storage.clear();
            yCInfo(MAP2DSERVER) << "All locations deleted";
            out.addVocab(VOCAB_OK);
//             ret = true;
        }
        else if (cmd == VOCAB_NAV_CLEARALL_X && in.get(2).asVocab() == VOCAB_NAV_AREA)
        {
            m_areas_storage.clear();
            yCInfo(MAP2DSERVER) << "All areas deleted";
            out.addVocab(VOCAB_OK);
            //             ret = true;
        }
        else if (cmd == VOCAB_NAV_CLEARALL_X && in.get(2).asVocab() == VOCAB_NAV_PATH)
        {
            m_paths_storage.clear();
            yCInfo(MAP2DSERVER) << "All paths deleted";
            out.addVocab(VOCAB_OK);
            //             ret = true;
        }
        else if (cmd == VOCAB_NAV_CLEARALL_X && in.get(2).asVocab() == VOCAB_NAV_TEMPORARY_FLAGS)
        {
            for (auto it = m_maps_storage.begin(); it != m_maps_storage.end(); it++)
            {
                it->second.clearMapTemporaryFlags();
            }
            yCInfo(MAP2DSERVER) << "Temporary flags deleted from all maps";
            out.addVocab(VOCAB_OK);
            //             ret = true;
        }
        else if (cmd == VOCAB_NAV_DELETE_X && in.get(2).asVocab() == VOCAB_NAV_TEMPORARY_FLAGS)
        {
            std::string name = in.get(3).asString();

            auto it = m_maps_storage.find(name);
            if (it != m_maps_storage.end())
            {
                yCInfo(MAP2DSERVER) << "Temporary flags cleaned" << name;
                it->second.clearMapTemporaryFlags();
                out.addVocab(VOCAB_OK);
            }
            else
            {
                yCError(MAP2DSERVER, "User requested an invalid map name");
                out.addVocab(VOCAB_ERR);
            }
            //             ret = true;
        }
        else if (cmd == VOCAB_NAV_DELETE_X && in.get(2).asVocab() == VOCAB_NAV_LOCATION)
        {
            std::string name = in.get(3).asString();

            std::map<std::string, Map2DLocation>::iterator it;
            it = m_locations_storage.find(name);
            if (it != m_locations_storage.end())
            {
                yCInfo(MAP2DSERVER) << "Deleted location" << name;
                m_locations_storage.erase(it);
                out.addVocab(VOCAB_OK);
            }
            else
            {
                yCError(MAP2DSERVER, "User requested an invalid location name");
                out.addVocab(VOCAB_ERR);
            }
//             ret = true;
        }
        else if (cmd == VOCAB_NAV_DELETE_X && in.get(2).asVocab() == VOCAB_NAV_PATH)
        {
            std::string name = in.get(3).asString();

            std::map<std::string, Map2DPath>::iterator it;
            it = m_paths_storage.find(name);
            if (it != m_paths_storage.end())
            {
                yCInfo(MAP2DSERVER) << "Deleted path" << name;
                m_paths_storage.erase(it);
                out.addVocab(VOCAB_OK);
            }
            else
            {
                yCError(MAP2DSERVER, "User requested an invalid location name");
                out.addVocab(VOCAB_ERR);
            }

            //             ret = true;
        }
        else if (cmd == VOCAB_NAV_RENAME_X && in.get(2).asVocab() == VOCAB_NAV_LOCATION)
        {
            std::string orig_name = in.get(3).asString();
            std::string new_name = in.get(4).asString();

            std::map<std::string, Map2DLocation>::iterator orig_it;
            orig_it = m_locations_storage.find(orig_name);
            std::map<std::string, Map2DLocation>::iterator new_it;
            new_it = m_locations_storage.find(new_name);

            if (orig_it != m_locations_storage.end() &&
                new_it  == m_locations_storage.end())
            {
                yCInfo(MAP2DSERVER) << "Location:" << orig_name << "renamed to:" << new_name;
                auto loc = orig_it->second;
                m_locations_storage.erase(orig_it);
                m_locations_storage.insert(std::pair<std::string, Map2DLocation>(new_name, loc));
                out.addVocab(VOCAB_OK);
            }
            else
            {
                yCError(MAP2DSERVER, "User requested an invalid rename operation");
                out.addVocab(VOCAB_ERR);
            }
            //             ret = true;
        }
        else if (cmd == VOCAB_NAV_RENAME_X && in.get(2).asVocab() == VOCAB_NAV_AREA)
        {
            std::string orig_name = in.get(3).asString();
            std::string new_name = in.get(4).asString();

            std::map<std::string, Map2DArea>::iterator orig_it;
            orig_it = m_areas_storage.find(orig_name);
            std::map<std::string, Map2DArea>::iterator new_it;
            new_it = m_areas_storage.find(new_name);

            if (orig_it != m_areas_storage.end() &&
                new_it == m_areas_storage.end())
            {
                yCInfo(MAP2DSERVER) << "Area:" << orig_name << "renamed to:" << new_name;
                auto area = orig_it->second;
                m_areas_storage.erase(orig_it);
                m_areas_storage.insert(std::pair<std::string, Map2DArea>(new_name,area));
                out.addVocab(VOCAB_OK);
            }
            else
            {
                yCError(MAP2DSERVER, "User requested an invalid rename operation");
                out.addVocab(VOCAB_ERR);
            }
//             ret = true;
        }
        else if (cmd == VOCAB_NAV_RENAME_X && in.get(2).asVocab() == VOCAB_NAV_PATH)
        {
            std::string orig_name = in.get(3).asString();
            std::string new_name = in.get(4).asString();

            std::map<std::string, Map2DPath>::iterator orig_it;
            orig_it = m_paths_storage.find(orig_name);
            std::map<std::string, Map2DPath>::iterator new_it;
            new_it = m_paths_storage.find(new_name);

            if (orig_it != m_paths_storage.end() &&
                new_it == m_paths_storage.end())
            {
                yCInfo(MAP2DSERVER) << "Path:" << orig_name << "renamed to:" << new_name;
                auto area = orig_it->second;
                m_paths_storage.erase(orig_it);
                m_paths_storage.insert(std::pair<std::string, Map2DPath>(new_name, area));
                out.addVocab(VOCAB_OK);
            }
            else
            {
                yCError(MAP2DSERVER, "User requested an invalid rename operation");
                out.addVocab(VOCAB_ERR);
            }
            //             ret = true;
        }
        else if (cmd == VOCAB_NAV_DELETE_X && in.get(2).asVocab() == VOCAB_NAV_AREA)
        {
        std::string name = in.get(3).asString();

        std::map<std::string, Map2DArea>::iterator it;
        it = m_areas_storage.find(name);
        if (it != m_areas_storage.end())
        {
            yCInfo(MAP2DSERVER) << "Deleted area" << name;
            m_areas_storage.erase(it);
            out.addVocab(VOCAB_OK);
        }
        else
        {
            yCError(MAP2DSERVER, "User requested an invalid area name");
            out.addVocab(VOCAB_ERR);
        }

        //             ret = true;
        }
        else if (cmd == VOCAB_NAV_GET_X && in.get(2).asVocab() == VOCAB_NAV_LOCATION)
        {
            std::string name = in.get(3).asString();

            std::map<std::string, Map2DLocation>::iterator it;
            it = m_locations_storage.find(name);
            if (it != m_locations_storage.end())
            {
                out.addVocab(VOCAB_OK);
                Map2DLocation loc = it->second;
                yCInfo(MAP2DSERVER) << "Retrieved location" << name << "at" << loc.toString();
                out.addString(loc.map_id);
                out.addFloat64(loc.x);
                out.addFloat64(loc.y);
                out.addFloat64(loc.theta);
            }
            else
            {
                out.addVocab(VOCAB_ERR);
                yCError(MAP2DSERVER, "User requested an invalid location name");
            }
            //ret = true;
        }
        else if (cmd == VOCAB_NAV_GET_X && in.get(2).asVocab() == VOCAB_NAV_AREA)
        {
            std::string area_name = in.get(3).asString();

            std::map<std::string, Map2DArea>::iterator it;
            it = m_areas_storage.find(area_name);
            if (it != m_areas_storage.end())
            {
                Map2DArea area = it->second;
                yarp::os::Bottle areabot;
                Map2DArea areatemp = area;
                if (Property::copyPortable(areatemp, areabot) == false)
                {
                    yCError(MAP2DSERVER) << "VOCAB_NAV_GET_X VOCAB_NAV_AREA failed copyPortable()";
                    out.addVocab(VOCAB_ERR);
                }
                else
                {
                    yCInfo(MAP2DSERVER) << "Retrieved area" << area_name << "at" << area.toString();
                    out.addVocab(VOCAB_OK);

                    yarp::os::Bottle& areabot = out.addList();
                    Property::copyPortable(areatemp, areabot);
                }
            }
            else
            {
                out.addVocab(VOCAB_ERR);
                yCError(MAP2DSERVER, "User requested an invalid area name");
            }
            //ret = true;
        }
        else if (cmd == VOCAB_NAV_GET_X && in.get(2).asVocab() == VOCAB_NAV_PATH)
        {
        std::string path_name = in.get(3).asString();

        std::map<std::string, Map2DPath>::iterator it;
        it = m_paths_storage.find(path_name);
        if (it != m_paths_storage.end())
        {
            Map2DPath path = it->second;
            yarp::os::Bottle pathbot;
            Map2DPath pathtemp = path;
            if (Property::copyPortable(pathtemp, pathbot) == false)
            {
                yCError(MAP2DSERVER) << "VOCAB_NAV_GET_X VOCAB_NAV_PATH failed copyPortable()";
                out.addVocab(VOCAB_ERR);
            }
            else
            {
                yCInfo(MAP2DSERVER) << "Retrieved path" << path_name << "at" << path.toString();
                out.addVocab(VOCAB_OK);

                yarp::os::Bottle& pathbot = out.addList();
                Property::copyPortable(pathtemp, pathbot);
            }
        }
        else
        {
            out.addVocab(VOCAB_ERR);
            yCError(MAP2DSERVER, "User requested an invalid path name");
        }
        //ret = true;
        }
        else if (cmd == VOCAB_NAV_STORE_X && in.get(2).asVocab() == VOCAB_NAV_LOCATION)
        {
            Map2DLocation         location;
            std::string name = in.get(3).asString();

            location.map_id = in.get(4).asString();
            location.x      = in.get(5).asFloat64();
            location.y      = in.get(6).asFloat64();
            location.theta  = in.get(7).asFloat64();

            m_locations_storage.insert(std::pair<std::string, Map2DLocation>(name, location));
            yCInfo(MAP2DSERVER) << "Added location" << name << "at" << location.toString();
            out.addVocab(VOCAB_OK);
            //ret = true;
        }
        else if (cmd == VOCAB_NAV_STORE_X && in.get(2).asVocab() == VOCAB_NAV_AREA)
        {
            Map2DArea         area;
            std::string area_name = in.get(3).asString();

            Value& b = in.get(4);
            if (Property::copyPortable(b, area))
            {
                m_areas_storage.insert(std::pair<std::string, Map2DArea>(area_name, area));
                yCInfo(MAP2DSERVER) << "Added area" << area_name << "at" << area.toString();
                out.addVocab(VOCAB_OK);
            }
            else
            {
                yCError(MAP2DSERVER) << "VOCAB_NAV_STORE_X VOCAB_NAV_AREA failed copyPortable()";
                out.addVocab(VOCAB_ERR);
            }
            //ret = true;
        }
        else if (cmd == VOCAB_NAV_STORE_X && in.get(2).asVocab() == VOCAB_NAV_PATH)
        {
            Map2DPath         path;
            std::string path_name = in.get(3).asString();

            Value& b = in.get(4);
            if (Property::copyPortable(b, path))
            {
                m_paths_storage.insert(std::pair<std::string, Map2DPath>(path_name, path));
                yCInfo(MAP2DSERVER) << "Added path" << path_name << "at" << path.toString();
                out.addVocab(VOCAB_OK);
            }
            else
            {
                yCError(MAP2DSERVER) << "VOCAB_NAV_STORE_X VOCAB_NAV_PATH failed copyPortable()";
                out.addVocab(VOCAB_ERR);
            }
            //ret = true;
        }
        else
        {
            yCError(MAP2DSERVER) << "Invalid vocab received:" << in.toString();
            out.clear();
            out.addVocab(VOCAB_ERR);
        }
    }
    else
    {
        yCError(MAP2DSERVER) << "Invalid vocab received:" << in.toString();
        out.clear();
        out.addVocab(VOCAB_IMAP_ERROR);
    }
}

void Map2D_nws_yarp::parse_string_command(yarp::os::Bottle& in, yarp::os::Bottle& out)
{
    if (in.get(0).asString() == "save_locations&areas" && in.get(1).isString())
    {
        if(save_locations_and_areas(in.get(1).asString()))
        {
            out.addString(in.get(1).asString() + " successfully saved");
        }
        else
        {
            out.addString("save_locations&areas failed");
        }
    }
    else if (in.get(0).asString() == "load_locations&areas" && in.get(1).isString())
    {
        if(load_locations_and_areas(in.get(1).asString()))
        {
            out.addString(in.get(1).asString() + " successfully loaded");
        }
        else
        {
            out.addString("load_locations&areas failed");
        }
    }
    else if(in.get(0).asString() == "list_locations")
    {
        std::map<std::string, Map2DLocation>::iterator it;
        for (it = m_locations_storage.begin(); it != m_locations_storage.end(); ++it)
        {
            out.addString(it->first);
        }
    }
    else if (in.get(0).asString() == "list_areas")
    {
        std::map<std::string, Map2DArea>::iterator it;
        for (it = m_areas_storage.begin(); it != m_areas_storage.end(); ++it)
        {
            out.addString(it->first);
        }
    }
    else if (in.get(0).asString() == "list_paths")
    {
        std::map<std::string, Map2DPath>::iterator it;
        for (it = m_paths_storage.begin(); it != m_paths_storage.end(); ++it)
        {
            out.addString(it->first);
        }
    }
    else if (in.get(0).asString() == "save_maps" && in.get(1).isString())
    {
        if(saveMaps(in.get(1).asString()))
        {
            out.addString(in.get(1).asString() + " successfully saved");
        }
        else
        {
            out.addString("save_maps failed");
        }
    }
    else if (in.get(0).asString() == "load_maps" && in.get(1).isString())
    {
        if(loadMaps(in.get(1).asString()))
        {
            out.addString(in.get(1).asString() + " successfully loaded");
        }
        else
        {
            out.addString("load_maps failed");
        }
    }
    else if (in.get(0).asString() == "save_map" && in.get(1).isString() && in.get(2).isString())
    {
        std::string map_name = in.get(1).asString();
        std::string map_file = in.get(2).asString() + ".map";
        auto p = m_maps_storage.find(map_name);
        if (p == m_maps_storage.end())
        {
            out.addString("save_map failed: map " + map_name + " not found");
        }
        else
        {
            bool b = p->second.saveToFile(map_file);
            if (b)
            {
                out.addString(map_file + " successfully saved");
            }
            else
            {
                out.addString("save_map failed: unable to save " + map_name + " to "+ map_file);
            }
        }
    }
    else if (in.get(0).asString() == "load_map" && in.get(1).isString())
    {
        MapGrid2D map;
        bool r = map.loadFromFile(in.get(1).asString());
        if(r)
        {
            string map_name= map.getMapName();
            auto p = m_maps_storage.find(map_name);
            if (p == m_maps_storage.end())
            {
                m_maps_storage[map_name] = map;
                out.addString(in.get(1).asString() + " successfully loaded.");
            }
            else
            {
                out.addString(in.get(1).asString() + " already exists, skipping.");
            }
        }
        else
        {
            out.addString("load_map failed. Unable to load " + in.get(1).asString());
        }
    }
    else if(in.get(0).asString() == "list_maps")
    {
        std::map<std::string, MapGrid2D>::iterator it;
        for (it = m_maps_storage.begin(); it != m_maps_storage.end(); ++it)
        {
            out.addString(it->first);
        }
    }
    else if(in.get(0).asString() == "clear_all_locations")
    {
        m_locations_storage.clear();
        out.addString("all locations cleared");
    }
    else if (in.get(0).asString() == "clear_all_areas")
    {
        m_areas_storage.clear();
        out.addString("all areas cleared");
    }
    else if (in.get(0).asString() == "clear_all_paths")
    {
        m_paths_storage.clear();
        out.addString("all paths cleared");
    }
    else if(in.get(0).asString() == "clear_all_maps")
    {
        m_maps_storage.clear();
        out.addString("all maps cleared");
    }
    else if (in.get(0).asString() == "enable_maps_compression")
    {
        bool b = true;
        for (auto it=m_maps_storage.begin(); it!= m_maps_storage.end(); it++)
            {b &= it->second.enable_map_compression_over_network(in.get(1).asBool());}
        if (b) {out.addString("compression mode of all maps set to:"+ in.get(1).asString());}
        else   {out.addString("failed to set compression mode");}
    }
    else if(in.get(0).asString() == "help")
    {
        out.addVocab(Vocab::encode("many"));
        out.addString("'save_locations&areas <full path filename>' to save locations and areas on a file");
        out.addString("'load_locations&areas <full path filename>' to load locations and areas from a file");
        out.addString("'list_locations' to view a list of all stored locations");
        out.addString("'list_areas' to view a list of all stored areas");
        out.addString("'list_paths' to view a list of all stored paths");
        out.addString("'clear_all_locations' to clear all stored locations");
        out.addString("'clear_all_areas' to clear all stored areas");
        out.addString("'clear_all_paths' to clear all stored paths");
        out.addString("'save_maps <full path>' to save a map collection to a folder");
        out.addString("'load_maps <full path>' to load a map collection from a folder");
        out.addString("'save_map <map_name> <full path>' to save a single map");
        out.addString("'load_map <full path>' to load a single map");
        out.addString("'list_maps' to view a list of all stored maps");
        out.addString("'clear_all_maps' to clear all stored maps");
        out.addString("'enable_maps_compression <0/1>' to set the map transmission mode");
    }
    else
    {
        out.addString("request not understood, call 'help' to see a list of available commands");
    }

    //updateVizMarkers();
}

bool Map2D_nws_yarp::read(yarp::os::ConnectionReader& connection)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    yarp::os::Bottle in;
    yarp::os::Bottle out;
    bool ok = in.read(connection);
    if (!ok) return false;

    //parse string command
    if(in.get(0).isString())
    {
        parse_string_command(in, out);
    }
    // parse vocab command
    else if(in.get(0).isVocab())
    {
        parse_vocab_command(in, out);
    }

    yarp::os::ConnectionWriter *returnToSender = connection.getWriter();
    if (returnToSender != nullptr)
    {
        out.write(*returnToSender);
    }
    else
    {
        yCError(MAP2DSERVER) << "Invalid return to sender";
    }
    return true;
}

bool Map2D_nws_yarp::saveMaps(std::string mapsfile)
{
    if (m_maps_storage.size() == 0)
    {
        yCError(MAP2DSERVER) << "Map storage is empty";
        return false;
    }
    std::ofstream file;
    file.open(mapsfile.c_str());
    if (!file.is_open())
    {
        yCError(MAP2DSERVER) << "Sorry unable to open" << mapsfile;
        return false;
    }
    bool ret = true;
    for (auto& it : m_maps_storage)
    {
        string map_filename = it.first + ".map";
        file << "mapfile: ";
        file << map_filename;
        file << endl;
        ret &= it.second.saveToFile(map_filename);
    }
    file.close();
    return ret;
}

bool Map2D_nws_yarp::loadMaps(std::string mapsfile)
{
    bool ret = true;
    std::ifstream file;
    file.open(mapsfile.c_str());
    if (!file.is_open())
    {
        yCError(MAP2DSERVER) << "loadMaps() Unable to open:" << mapsfile;
        return false;
    }
    while (!file.eof())
    {
        string dummy;
        string buffer;
        std::getline(file, buffer);
        std::istringstream iss(buffer);
        iss >> dummy;
        if (dummy == "") break;
        if (dummy == "mapfile:")
        {
            string mapfilename;
            iss >> mapfilename;
            string option;
            iss >> option;
            string mapfilenameWithPath = m_rf_mapCollection.findFile(mapfilename);
            MapGrid2D map;
            bool r = map.loadFromFile(mapfilenameWithPath);
            if (r)
            {
                string map_name= map.getMapName();
                auto p = m_maps_storage.find(map_name);
                if (p == m_maps_storage.end())
                {
                    if (option == "crop")
                        map.crop(-1,-1,-1,-1);
                    m_maps_storage[map_name] = map;
                }
                else
                {
                    yCError(MAP2DSERVER) << "A map with the same name '" << map_name << "'was found, skipping...";
                    ret = false;
                }
            }
            else
            {
                yCError(MAP2DSERVER) << "Problems opening map file" << mapfilenameWithPath;
                ret = false;
            }
        }
        else
        {
            yCError(MAP2DSERVER) << "Invalid syntax, missing mapfile tag";
            ret = false;
        }
    }
    file.close();
    return ret;
}

bool Map2D_nws_yarp::open(yarp::os::Searchable &config)
{
    Property params;
    params.fromString(config.toString());

    string collection_file_name="maps_collection.ini";
    string locations_file_name="locations.ini";
    if (config.check("mapCollectionFile"))
    {
        collection_file_name= config.find("mapCollectionFile").asString();
    }

    if (config.check("mapCollectionContext"))
    {
        string collection_context_name= config.find("mapCollectionContext").asString();
        m_rf_mapCollection.setDefaultContext(collection_context_name.c_str());
        string collection_file_with_path = m_rf_mapCollection.findFile(collection_file_name);
        string locations_file_with_path = m_rf_mapCollection.findFile(locations_file_name);

        if (collection_file_with_path=="")
        {
            yCInfo(MAP2DSERVER) << "No locations loaded";
        }
        else
        {
            bool ret  = load_locations_and_areas(locations_file_with_path);
            if (ret) { yCInfo(MAP2DSERVER) << "Location file" << locations_file_with_path << "successfully loaded."; }
            else { yCError(MAP2DSERVER) << "Problems opening file" << locations_file_with_path; }
        }

        if (collection_file_with_path=="")
        {
            yCError(MAP2DSERVER) << "Unable to find file" << collection_file_name << "within the specified context:" << collection_context_name;
            return false;
        }
        if (loadMaps(collection_file_with_path))
        {
            yCInfo(MAP2DSERVER) << "Map collection file:" << collection_file_with_path << "successfully loaded.";
            if (m_maps_storage.size() > 0)
            {
                yCInfo(MAP2DSERVER) << "Available maps are:";
                for (auto& it : m_maps_storage)
                {
                    yCInfo(MAP2DSERVER) << it.first;
                }
            }
            else
            {
                yCInfo(MAP2DSERVER) << "No maps available";
            }
            if (m_locations_storage.size() > 0)
            {
                yCInfo(MAP2DSERVER) << "Available Locations are:";
                for (auto& it : m_locations_storage)
                {
                    yCInfo(MAP2DSERVER) << it.first;
                }
            }
            else
            {
                yCInfo(MAP2DSERVER) << "No locations available";
            }

            if (m_areas_storage.size() > 0)
            {
                yCInfo(MAP2DSERVER) << "Available areas are:";
                for (auto& it : m_areas_storage)
                {
                    yCInfo(MAP2DSERVER) << it.first;
                }
            }
            else
            {
                yCInfo(MAP2DSERVER) << "No areas available";
            }
        }
        else
        {
            yCError(MAP2DSERVER) << "Unable to load map collection file:" << collection_file_with_path;
            return false;
        }
    }

    if (!config.check("name"))
    {
        m_rpcPortName = "/mapServer/rpc";
    }
    else
    {
        m_rpcPortName = config.find("name").asString();
    }

    //open rpc port
    if (!m_rpcPort.open(m_rpcPortName))
    {
        yCError(MAP2DSERVER, "Failed to open port %s", m_rpcPortName.c_str());
        return false;
    }
    m_rpcPort.setReader(*this);

    return true;
}

bool Map2D_nws_yarp::close()
{
    yCTrace(MAP2DSERVER, "Close");
    return true;
}

bool Map2D_nws_yarp::priv_load_locations_and_areas_v1(std::ifstream& file)
{
    std::string     buffer;
    std::getline(file, buffer);
    if (buffer != "Locations:")
    {
        yCError(MAP2DSERVER) << "Unable to parse Locations section!";
        return false;
    }

    while (1)
    {
        std::getline(file, buffer);
        if (buffer == "Areas:") break;
        if (file.eof())
        {
            yCError(MAP2DSERVER) << "Unexpected End Of File";
            return false;
        }
        Bottle b;
        b.fromString(buffer);
        size_t bot_size = b.size();
        if (bot_size != 5)
        {
            yCError(MAP2DSERVER) << "Unable to parse contents of Areas section!";
            return false;
        }
        Map2DLocation   location;
        std::string name = b.get(0).asString();
        location.map_id = b.get(1).asString();
        location.x = b.get(2).asFloat64();
        location.y = b.get(3).asFloat64();
        location.theta = b.get(4).asFloat64();
        m_locations_storage[name] = location;
    }

    if (buffer != "Areas:")
    {
        yCError(MAP2DSERVER) << "Unable to parse Areas section!";
        return false;
    }

    while (1)
    {
        Map2DArea       area;
        std::getline(file, buffer);
        if (file.eof()) break;

        Bottle b;
        b.fromString(buffer);
        size_t bot_size = b.size();
        std::string name = b.get(0).asString();
        area.map_id = b.get(1).asString();
        size_t area_size = b.get(2).asInt32();
        if (area_size <= 0 || bot_size != area_size * 2 + 3)
        {
            yCError(MAP2DSERVER) << "Unable to parse contents of Areas section!";
            return false;
        }
        for (size_t ai = 3; ai < bot_size; ai += 2)
        {
            double xpos = b.get(ai).asFloat64();
            double ypos = b.get(ai + 1).asFloat64();
            area.points.push_back(yarp::math::Vec2D<double>(xpos, ypos));
        }
        m_areas_storage[name] = area;
    }
    return true;
}

bool Map2D_nws_yarp::priv_load_locations_and_areas_v2(std::ifstream& file)
{
    std::string     buffer;
    std::getline(file, buffer);
    if (buffer != "Locations:")
    {
        yCError(MAP2DSERVER) << "Unable to parse Locations section!";
        return false;
    }

    while (1)
    {
        std::getline(file, buffer);
        if (buffer == "Areas:") break;
        if (file.eof())
        {
            yCError(MAP2DSERVER) << "Unexpected End Of File";
            return false;
        }
        Bottle b;
        b.fromString(buffer);
        size_t bot_size = b.size();
        if (bot_size != 5)
        {
            yCError(MAP2DSERVER) << "Unable to parse contents of Areas section!";
            return false;
        }
        Map2DLocation   location;
        std::string name = b.get(0).asString();
        location.map_id = b.get(1).asString();
        location.x = b.get(2).asFloat64();
        location.y = b.get(3).asFloat64();
        location.theta = b.get(4).asFloat64();
        m_locations_storage[name] = location;
    }

    if (buffer != "Areas:")
    {
        yCError(MAP2DSERVER) << "Unable to parse Areas section!";
        return false;
    }

    while (1)
    {
        Map2DArea       area;
        std::getline(file, buffer);
        if (file.eof()) break;

        Bottle b;
        b.fromString(buffer);
        size_t bot_size = b.size();
        std::string name = b.get(0).asString();
        area.map_id = b.get(1).asString();
        size_t area_size = b.get(2).asInt32();
        if (area_size <= 0 || bot_size != area_size * 2 + 3)
        {
            yCError(MAP2DSERVER) << "Unable to parse contents of Areas section!";
            return false;
        }
        for (size_t ai = 3; ai < bot_size; ai += 2)
        {
            double xpos = b.get(ai).asFloat64();
            double ypos = b.get(ai + 1).asFloat64();
            area.points.push_back(yarp::math::Vec2D<double>(xpos, ypos));
        }
        m_areas_storage[name] = area;
    }
    return true;
}

bool Map2D_nws_yarp::load_locations_and_areas(std::string locations_file)
{
    std::ifstream file;
    file.open (locations_file.c_str());

    if(!file.is_open())
    {
        yCError(MAP2DSERVER) << "Unable to open" << locations_file << "locations file.";
        return false;
    }

    std::string     buffer;

    std::getline(file, buffer);
    if (buffer != "Version:")
    {
        yCError(MAP2DSERVER) << "Unable to parse Version section!";
        file.close();
        return false;
    }
    std::getline(file, buffer);
    int version = atoi(buffer.c_str());

    if (version == 1)
    {
        if (!priv_load_locations_and_areas_v1(file))
        {
            yCError(MAP2DSERVER) << "Call to load_locations_and_areas_v1 failed";
            file.close();
            return false;
        }
    }
    else if (version == 2)
    {
        if (!priv_load_locations_and_areas_v2(file))
        {
            yCError(MAP2DSERVER) << "Call to load_locations_and_areas_v2 failed";
            file.close();
            return false;
        }
    }
    else
    {
        yCError(MAP2DSERVER) << "Only versions 1,2 supported!";
        file.close();
        return false;
    }

    //on success
    file.close();
    yCDebug(MAP2DSERVER) << "Locations file" << locations_file << "loaded, " << m_locations_storage.size() << "locations and "<< m_areas_storage.size() << " areas available";
    return true;
}

bool Map2D_nws_yarp::save_locations_and_areas(std::string locations_file)
{
    std::ofstream file;
    file.open (locations_file.c_str());

    if(!file.is_open())
    {
        yCError(MAP2DSERVER) << "Unable to open" << locations_file << "locations file.";
        return false;
    }

    std::string     s;
    Map2DLocation   loc;
    Map2DArea       area;
    s = " ";

    file << "Version: \n";
    file << "2\n";

    {
        file << "Locations: \n";
        std::map<std::string, Map2DLocation>::iterator it;
        for (it = m_locations_storage.begin(); it != m_locations_storage.end(); ++it)
        {
            loc = it->second;
            file << it->first << s << loc.map_id << s << loc.x << s << loc.y << s << loc.theta << "\n";
        }
    }

    {
        file << "Areas: \n";
        std::map<std::string, Map2DArea>::iterator it2;
        for (it2 = m_areas_storage.begin(); it2 != m_areas_storage.end(); ++it2)
        {
            area = it2->second;
            file << it2->first << s << area.map_id << s << area.points.size() << s;
            for (size_t i = 0; i < area.points.size(); i++)
            {
                file << area.points[i].x << s;
                file << area.points[i].y << s;
            }
            file << "\n";
        }
    }

    {
        file << "Paths: \n";
        std::map<std::string, Map2DPath>::iterator it3;
        for (it3 = m_paths_storage.begin(); it3 != m_paths_storage.end(); ++it3)
        {
            file << it3->first; // the name of the path
            for (size_t i=0; i<it3->second.size(); i++)
            {
                loc = it3->second[i];
                file << "( " <<loc.map_id << s << loc.x << s << loc.y << s << loc.theta << ") ";
            }
            file << "\n";
        }
    }

    file.close();
    yCDebug(MAP2DSERVER) << "Locations file" << locations_file << "saved.";
    return true;
}
