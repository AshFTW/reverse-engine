/*
    This file is part of Reverse Engine.

    Find process by PID or title, access it's address space, change any
    value you need.

    Copyright (C) 2017-2018 Ivan Stepanov <ivanstepanovftw@gmail.com>

    This library is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <sys/uio.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include <cstring>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <regex>
#include <sys/stat.h>
#include <boost/iostreams/device/mapped_file.hpp>
#include <reverseengine/value.hh>
#include <reverseengine/external.hh>
#include <reverseengine/common.hh>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/vector.hpp>


NAMESPACE_BEGIN(RE)

namespace sfs = std::filesystem;
namespace bio = boost::iostreams;


enum class region_mode_t : uint8_t
{
    none = 0,
    executable = 1u<<0u,
    writable   = 1u<<1u,
    readable   = 1u<<2u,
    shared     = 1u<<3u,
    max = executable|writable|readable|shared
};
BITMASK_DEFINE_MAX_ELEMENT(region_mode_t, max)




class Cregion {
public:
    uintptr_t address;
    uintptr_t size{};

    bitmask::bitmask<region_mode_t> flags;

    /// File data
    uintptr_t offset{};
    union {
        struct {
            uint8_t st_device_minor;
            uint8_t st_device_major;
        };
        __dev_t st_device;
    };
    unsigned long inodeFileNumber{}; //fixme unsigned long?
    std::string pathname; //fixme pathname?
    std::string filename;

    Cregion()
    : address(0), size(0), flags(region_mode_t::none), offset(0), st_device(0), inodeFileNumber(0) {}

    bool is_good() {
        return address != 0;
    }

    std::string str() const {
        std::ostringstream ss;
        ss << HEX(address)<<"-"<<HEX(address+size)<<" "
           << (flags & region_mode_t::shared?"s":"-") << (flags & region_mode_t::readable?"r":"-") << (flags & region_mode_t::writable?"w":"-") << (flags & region_mode_t::executable?"x":"-") << " "
           << HEX(st_device_major)<<":"<< HEX(st_device_minor) <<" "
           << inodeFileNumber<<" "
           << pathname;
        return ss.str();
    }

    friend std::ostream& operator<<(std::ostream& outputStream, const Cregion& region) {
        return outputStream<<region.str();
    }
private:
    friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive& ar, const unsigned int version) {
        ar & address;
        ar & size;
        ar & *reinterpret_cast<decltype(flags)::underlying_type*>(&flags);
        ar & offset;
        ar & st_device_minor;
        ar & st_device_major;
        ar & inodeFileNumber;
        ar & pathname;
        ar & filename;
    }
};

/********************
 *  Process handler
 ********************/

class phandler_i {
public:
    phandler_i() = default;
    ~phandler_i() = default;

    virtual size_t read(uintptr_t address, void *out, size_t size) const = 0;
    virtual size_t write(uintptr_t address, void *in, size_t size) const = 0;
    virtual bool is_valid() const = 0;
    virtual void update_regions() = 0;
    virtual const bool operator!() = 0;

    Cregion *get_region_by_name(const std::string& region_name) {
        for (Cregion& region : regions)
            if (region.flags & region_mode_t::executable && region.filename == region_name)
//                             ^~~~~~~~~~~~~~~~~~~~~~~~~ wtf? fixme[medium]: add documentation or die();
                return &region;
        return nullptr;
    }

    /*FIXME[CRITICAL]: REMAKE THIS SHIT, DO TESTS!!!!!!!!!!!!!! */
    size_t get_region_of_address(uintptr_t address) const {
        using namespace std;
        //static size_t last_return = npos;
        //if (last_return != npos
        //&& regions[last_return].address <= address
        //&& address < regions[last_return].address + regions[last_return].size) {
        //    //clog << "returning last region " << endl;
        //    return last_return;
        //}
        //cout<<"address: 0x"<<RE::HEX(address)<<", first region: 0x"<<RE::HEX(regions[0].address)<<" : 0x"<<RE::HEX(regions[0].address+regions[0].size)<<endl;
        size_t first, last, mid;
        first = 0;
        last = regions.size();
        //cout<<"last: "<<last<<endl;
        while (first <= last) {
            mid = (first + last) / 2;
            //cout<<"   mid: "<<mid<<endl;
            if (address < regions[mid].address) {
                last = mid - 1;
                //cout<<"      address at the left side of 0x"<<RE::HEX(regions[mid].address)<<endl;
            } else if (address >= regions[mid].address + regions[mid].size) {
                first = mid + 1;
                //cout<<"      address at the right side of 0x"<<RE::HEX(regions[mid].address + regions[mid].size)<<endl;
            } else {
                //last_return = mid;
                //return last_return;
                //std::cout<<"      returning mid: "<<mid<<std::endl;
                return mid;
            }
        }
        //std::cout<<"      returning npos: "<<mid<<std::endl;
        return npos;
    }

    uintptr_t get_call_address(uintptr_t address) const {
        uint64_t code = 0;
        if (read(address + 1, &code, sizeof(uint32_t)) == sizeof(uint32_t))
            return code + address + 5;
        return 0;
    }

    uintptr_t get_absolute_address(uintptr_t address, uintptr_t offset, uintptr_t size) const {
        uint64_t code = 0;
        if (read(address + offset, &code, sizeof(uint32_t)) == sizeof(uint32_t)) {
            return address + code + size;
        }
        return 0;
    }

public:
    /// Value returned by various member functions when they fail.
    static constexpr size_t npos = static_cast<size_t>(-1);

    std::vector<Cregion> regions;
    std::vector<Cregion> regions_ignored;
};


class handler_pid : public phandler_i {
public:
    using phandler_i::phandler_i;

    handler_pid() : m_pid(0) {}

    /** Make process handler from PID */
    explicit handler_pid(pid_t pid) noexcept {
        m_pid = pid;
        try {
            m_cmdline = get_executable().filename();
        } catch (const sfs::filesystem_error& e) {
            /* no such PID running */
            ;
        }
        if (m_cmdline.empty()) {
            m_pid = 0;
            m_cmdline = "";
            return;
        }
        update_regions();
    }

    /** Make process handler from process executable filename */
    explicit handler_pid(const std::string& title) {
        m_pid = 0;
        if (title.empty())
            return;

        for(auto& p: sfs::directory_iterator("/proc")) {
            if (p.path().filename().string().find_first_not_of("0123456789") != std::string::npos)
                /* if filename is not numeric */
                continue;
            if (!sfs::is_directory(p))
                continue;
            if (!sfs::exists(p / "maps"))
                continue;
            if (!sfs::exists(p / "exe"))
                continue;
            if (!sfs::exists(p / "cmdline"))
                continue;
            std::istringstream ss(p.path().filename().string());
            ss >> m_pid;
            m_cmdline = get_cmdline();

            std::regex rgx(title);
            std::smatch match;
            if (std::regex_search(m_cmdline, match, rgx)) {
                //std::cout<<"pid: "<<p.path().filename().string()<<", cmdline: "<<cmdline<<std::endl;
                //std::cout<<"{ ";
                //for (char* c = &cmdline[0]; c<&cmdline[0]+cmdline.length(); c++) {
                //    std::cout<< HEX<char, 1>(*c) <<" ";
                //}
                //std::cout<<"}";
                update_regions();
                return;
            }
        }
        m_pid = 0;
        m_cmdline = "";
    }

    /*!
     * Returns a symlink to the original executable file, if it still exists
     * (a process may continue running after its original executable has been deleted or replaced).
     * If the pathname has been unlinked, the symbolic link will contain the string ' (deleted)' appended to the original pathname.
     */
    [[gnu::always_inline]]
    sfs::path get_exe() {
        return sfs::read_symlink(sfs::path("/proc") / std::to_string(m_pid) / "exe");
    }

    /*!
     * Returns path to the original executable file, if it still exists, or throws std::runtime_error("File not found").
     */
    [[gnu::always_inline]]
    sfs::path get_executable() {
        /* Ambiguous */
        std::string exe = get_exe();
        for (const Cregion& region : regions) {
            if (region.pathname == exe) {
                struct stat sb{};
                errno = 0;
                int s = stat(exe.c_str(), &sb);
                if (s == 0 && sb.st_dev == region.st_device && sb.st_ino == region.inodeFileNumber)
                    return sfs::path(exe);
            }
        }
        throw std::runtime_error("File not found");
    }

    /** Process working directory real path */
    [[gnu::always_inline]]
    sfs::path get_working_directory() {
        return sfs::read_symlink(sfs::path("/proc") / std::to_string(m_pid) / "cwd");
    }

    /** Checking */
    [[gnu::always_inline]]
    bool is_valid() const override {
        return m_pid != 0;
    }

    /** Process working directory real path */
    [[gnu::always_inline]]
    bool is_running() const {
        return sfs::exists(sfs::path("/proc") / std::to_string(m_pid));
    }

    /** Read value */
    [[gnu::always_inline]]
    size_t read(uintptr_t address, void *out, size_t size) const override {
        struct iovec local[1];
        struct iovec remote[1];
        local[0].iov_base = out;
        local[0].iov_len = size;
        remote[0].iov_base = reinterpret_cast<void *>(address);
        remote[0].iov_len = size;
        return static_cast<size_t>(process_vm_readv(m_pid, local, 1, remote, 1, 0));
    }

    /** Write value */
    [[gnu::always_inline]]
    size_t write(uintptr_t address, void *in, size_t size) const override {
        struct iovec local[1];
        struct iovec remote[1];
        local[0].iov_base = in;
        local[0].iov_len = size;
        remote[0].iov_base = reinterpret_cast<void *>(address);
        remote[0].iov_len = size;
        return static_cast<size_t>(process_vm_writev(m_pid, local, 1, remote, 1, 0));
    }

    ///** Read value, return true if success */
    template <typename T, size_t _SIZE = sizeof(T)>
    bool read(uintptr_t address, T *out) {
        return read(address, out, _SIZE) == _SIZE;
    }

    ///** Read value, return true if success */
    template <typename T, size_t _SIZE = sizeof(T)>
    bool write(uintptr_t address, T *in) {
        return write(address, in, _SIZE) == _SIZE;
    }

    void update_regions() override {
        regions.clear();
        regions_ignored.clear();
        std::ifstream maps(sfs::path("/proc") / std::to_string(get_pid()) / "maps");
        std::string line;
        while (getline(maps, line)) {
            std::istringstream iss(line);
            std::string memorySpace, permissions, offset, device, inode;
            if (iss >> memorySpace >> permissions >> offset >> device >> inode) {
                std::string pathname;

                for (size_t ls = 0, i = 0; i < line.length(); i++) {
                    if (line.substr(i, 1) == " ") {
                        ls++;

                        if (ls == 5) {
                            size_t begin = line.substr(i, line.size()).find_first_not_of(' ');

                            if (begin != std::string::npos)
                                pathname = line.substr(begin + i, line.size());
                            else
                                pathname.clear();
                        }
                    }
                }

                Cregion region;

                size_t memorySplit = memorySpace.find_first_of('-');
                size_t deviceSplit = device.find_first_of(':');
                uintptr_t rend;

                std::stringstream ss;

                if (memorySplit != std::string::npos) {
                    ss << std::hex << memorySpace.substr(0, memorySplit);
                    ss >> region.address;
                    ss.clear();
                    ss << std::hex << memorySpace.substr(memorySplit + 1, memorySpace.size());
                    ss >> rend;
                    region.size = (region.address < rend) ? (rend - region.address) : 0;
                    ss.clear();
                }

                if (deviceSplit != std::string::npos) {
                    region.st_device_major = static_cast<uint8_t>(stoi(device.substr(0, deviceSplit), nullptr, 16));
                    region.st_device_minor = static_cast<uint8_t>(stoi(device.substr(deviceSplit + 1, device.size()), nullptr, 16));
                }

                ss << std::hex << offset;
                ss >> region.offset;
                ss.clear();
                ss << std::dec << inode;
                ss >> region.inodeFileNumber;

                region.flags = region_mode_t::none;
                region.flags |= (permissions[0] == 'r') ? region_mode_t::readable : region_mode_t::none;
                region.flags |= (permissions[1] == 'w') ? region_mode_t::writable : region_mode_t::none;
                region.flags |= (permissions[2] == 'x') ? region_mode_t::executable : region_mode_t::none;
                region.flags |= (permissions[3] == '-') ? region_mode_t::shared : region_mode_t::none;

                if (!pathname.empty()) {
                    region.pathname = pathname;

                    size_t fileNameSplit = pathname.find_last_of('/');

                    if (fileNameSplit != std::string::npos) {
                        region.filename = pathname.substr(fileNameSplit + 1, pathname.size());
                    }
                }

                if (region.flags & (region_mode_t::readable | region_mode_t::writable) && !(region.flags & region_mode_t::shared) && region.size > 0)
                    regions.push_back(region);
                else
                    regions_ignored.push_back(region);
            }
        }
        regions.shrink_to_fit();
        regions_ignored.shrink_to_fit();
    }

    constexpr pid_t get_pid() { return this->m_pid; }

    /** cmdline */
    [[gnu::always_inline]]
    std::string get_cmdline() {
        std::ifstream t(sfs::path("/proc") / std::to_string(m_pid) / "cmdline");
        std::getline(t, m_cmdline, '\0');
        return m_cmdline;
    }

    const bool operator!() override { return !is_valid() || !is_running(); }

private:
    pid_t m_pid;
    std::string m_cmdline;
};

class phandler_map_i : public phandler_i {
public:
    using phandler_i::phandler_i;

    /** Read value */
    [[gnu::always_inline]]
    size_t read(uintptr_t address, void *out, size_t size) const override {
        using namespace std;
        size_t r = get_region_of_address(address);
        if UNLIKELY(r == npos)
            return npos;
        size_t size_requested = size;
        if UNLIKELY(address + size > regions[r].address + regions[r].size) {
            size = regions[r].size - (address - regions[r].address);
            //cout<<"Trying to read "<<HEX(address)<<" of size "<<HEX(size)<<", but region "<<region<<std::endl; // DEBUG_STDOUT
            memset((char*)(out) + size, 0, (size_requested-size));
        }
        memcpy(out, reinterpret_cast<char *>(regions_on_map[r] + (address - regions[r].address)), size);
        return size;
    }

    /** Write value */
    [[gnu::always_inline]]
    size_t write(uintptr_t address, void *in, size_t size) const override {
        return static_cast<size_t>(size);
    }

    bool is_valid() const override {
        return false;
    }

    void update_regions() override {

    }

    const bool operator!() override {
        return 0;
    }

protected:
    std::vector<char *> regions_on_map;
};

class phandler_file;

class phandler_memory : public phandler_map_i {
public:
    using phandler_map_i::phandler_map_i;

    explicit phandler_memory(const handler_pid& rhs) {
        regions = rhs.regions;
        regions_ignored = rhs.regions_ignored;
        for (const RE::Cregion& region : rhs.regions) {
            char* map = new char[region.size];
            regions_on_map.emplace_back(map);
            ssize_t copied = rhs.read(region.address, map, region.size);
        }
    }

    explicit phandler_memory(const phandler_file& rhs);

    ~phandler_memory() {
        for (char* r : regions_on_map) {
            if (r) {
                delete[] r;
                r = nullptr;
            }
        }
        regions_on_map.clear();
    }

};


class phandler_file : public phandler_map_i {
public:
    using phandler_map_i::phandler_map_i;

    /*!
     * Now, we will able to send data to another PC and search pointers together.
     */
    void
    save(const handler_pid& handler, const std::string& path) {
        regions = handler.regions;

        std::ofstream stream(path, std::ios_base::out | std::ios_base::binary);
        boost::archive::binary_oarchive archive(stream, boost::archive::no_header);
        archive << *this;
        stream.flush();

        params.path = path;
        params.flags = bio::mapped_file::mapmode::readwrite;
        params.new_file_size = 0; //todo[critical]: to remove??
        mf.open(params);
        if (!mf.is_open())
            throw std::invalid_argument("can not open '" + path + "'");

        size_t bytes_to_save = 0;
        for (const RE::Cregion& region : handler.regions)
            bytes_to_save += sizeof(region.size) + region.size;
        mf.resize(mf.size() + bytes_to_save);

        char* snapshot = mf.data();
        assert(stream.is_open());
        assert(stream.tellp() != -1);
        snapshot += stream.tellp();

        regions_on_map.clear();
        for(const RE::Cregion& region : handler.regions) {
            regions_on_map.emplace_back(snapshot);
            ssize_t copied = handler.read(region.address, snapshot, region.size);
            //snapshot += region.size + sizeof(mem64_t::bytes) - 1;
            snapshot += region.size;
        }
    }

    void
    load(const std::string& path) {
        std::ifstream stream(path, std::ios_base::in | std::ios_base::binary);
        std::string m(magic);
        stream.read(&m[0], m.size());

        if (!stream || m != magic) {
            return;
        }
        boost::archive::binary_iarchive archive(stream, boost::archive::no_header);
        archive >> *this;

        params.path = path;
        params.flags = bio::mapped_file::mapmode::readwrite;
        mf.open(params);
        if (!mf.is_open())
            throw std::invalid_argument("can not open '" + path + "'");

        char* snapshot = mf.data();
        assert(stream.is_open());
        assert(stream.tellp() != -1);
        snapshot += stream.tellg();

        regions_on_map.clear();
        for (const RE::Cregion& region : regions) {
            regions_on_map.emplace_back(snapshot);
            //snapshot += region.size + sizeof(mem64_t::bytes) - 1;
            snapshot += region.size;
        }
    }

    /// Open (open snapshot from file)
    explicit phandler_file(const std::string& path) {
        this->load(path);
    }

    ~phandler_file() {
    }

    /// Save (make snapshot)
    explicit phandler_file(const handler_pid& handler, const std::string& path) {
        this->save(handler, path);
    }

    bool is_valid() const override {
        return false;
    }

    void update_regions() override {

    }

    const bool operator!() override {
        return 0;
    }

public:
    const std::string magic = "RE::phandler_file";

private:
    friend class boost::serialization::access;

    template<typename Archive>
    void serialize(Archive& ar, const unsigned int version) {
        ar & regions;
    }

private:
    bio::mapped_file mf;
    bio::mapped_file_params params;
};


/** The cached reader made for reading small values many times to reduce system calls */
template<class __PHANDLER>
class cached_reader {
public:
    explicit cached_reader(__PHANDLER& parent)
            : m_parent(parent) {
        m_base = 0;
        m_cache_size = 0;
        m_cache = new uint8_t[MAX_PEEKBUF_SIZE];
    }

    ~cached_reader() {
        delete[] m_cache;
    }

    [[gnu::always_inline]]
    void reset() {
        m_base = 0;
        m_cache_size = 0;
    }

    template<class PH = __PHANDLER, typename std::enable_if<std::is_base_of<PH, phandler_file>::value>::type* = nullptr>
    [[gnu::always_inline]]
    size_t read(uintptr_t address, void *out, size_t size) {
        if UNLIKELY(size > MAX_PEEKBUF_SIZE) {
            return m_parent.read(address, out, size);
        }

        if (m_base
            && address >= m_base
            && address - m_base + size <= m_cache_size) {
            /* full cache hit */
            memcpy(out, &m_cache[address - m_base], size);
            return size;
        }

        /* we need to retrieve memory to complete the request */
        size_t len = m_parent.read(address, &m_cache[0], MAX_PEEKBUF_SIZE);
        if (len == m_parent.npos) {
            /* hard failure to retrieve memory */
            reset();
            return m_parent.npos;
        }

        m_base = address;
        m_cache_size = len;

        /* return result to caller */
        memcpy(out, &m_cache[0], size);
        return MIN(size, m_cache_size);
    }

    template<class PH = __PHANDLER, typename std::enable_if<!std::is_base_of<PH, phandler_file>::value>::type* = nullptr>
    [[gnu::always_inline]]
    size_t read(uintptr_t address, void *out, size_t size) {
        return m_parent.read(address, out, size);
    }

public:
    static constexpr size_t MAX_PEEKBUF_SIZE = 4 * (1 << 10);

private:
    __PHANDLER& m_parent;
    uintptr_t m_base; // base address of cached region
    size_t m_cache_size;
    uint8_t *m_cache;
};


/*bool //todo короче можно вместо стрима закинуть, например, вектор со стрингами
handler::findPointer(void *out, uintptr_t address, std::vector<uintptr_t> offsets, size_t size, size_t point_size, std::ostream *ss)
{
    void *buffer;
    if (ss) *ss<<"["<<offsets[0]<<"] -> ";
    if (!this->read(&buffer, (void *)(address), point_size)) return false;
    if (ss) *ss<<""<<buffer<<endl;
    for(uint64_t i = 0; i < offsets.size()-1; i++) {
        if (ss) *ss<<"["<<buffer<<" + "<<offsets[i]<<"] -> ";
        if (!this->read(&buffer, buffer + offsets[i], point_size)) return false;
        if (ss) *ss<<""<<buffer<<endl;
    }
    if (ss) *ss<<buffer<<" + "<<offsets[offsets.size()-1]<<" = "<<buffer+offsets[offsets.size()-1]<<" -> ";
    if (!this->read(out, buffer + offsets[offsets.size() - 1], size)) return false;

    return true;
}

size_t
handler::findPattern(vector<uintptr_t> *out, Cregion *region, const char *pattern, const char *mask)
{
    char buffer[0x1000];

    size_t len = strlen(mask);
    size_t chunksize = sizeof(buffer);
    size_t totalsize = region->end - region->address;
    size_t chunknum = 0;
    size_t found = 0;

    while (totalsize) {
        size_t readsize = (totalsize < chunksize) ? totalsize : chunksize;
        size_t readaddr = region->address + (chunksize * chunknum);
        bzero(buffer, chunksize);

        if (this->read(buffer, (void *) readaddr, readsize)) {
            for(size_t b = 0; b < readsize; b++) {
                size_t matches = 0;

                // если данные совпадают или пропустить
                while (buffer[b + matches] == pattern[matches] || mask[matches] != 'x') {
                    matches++;

                    if (matches == len) {
                        found++;
                        out->push_back((uintptr_t) (readaddr + b));
                    }
                }
            }
        }

        totalsize -= readsize;
        chunknum++;
    }
    return found;
}

size_t
handler::scan_exact(vector<Entry> *out,
                   const Cregion *region,
                   vector<Entry> entries, 
                   size_t increment)
{
    byte buffer[0x1000];

    size_t chunksize = sizeof(buffer);
    size_t totalsize = region->end - region->address;
    size_t chunknum = 0;
    size_t found = 0;
    // TODO[HIGH] научить не добавлять, если предыдущий (собсна, наибольший) уже есть 
    while (totalsize) {
        size_t readsize = (totalsize < chunksize) ? totalsize : chunksize;
        uintptr_t readaddr = region->address + (chunksize * chunknum);
        bzero(buffer, chunksize);

        if (this->read(buffer, (void *) readaddr, readsize)) {    // read into buffer
            for(uintptr_t b = 0; b < readsize; b += increment) {  // for each addr inside buffer
                for(int k = 0; k < entries.size(); k++) {         // for each entry
                    size_t matches = 0;
                    while (buffer[b + matches] == entries[k].value.bytes[matches]) {  // находим адрес
                        matches++;

                        if (matches == SIZEOF_FLAG(entries[k].flags)) {
                            found++;
                            out->emplace_back(entries[k].flags, (uintptr_t)(readaddr + b), region, entries[k].value.bytes);
                            //todo мне кажется, что нужно всё-таки добавить плавующую точку, посмотрим, как сделаю scan_reset
                            goto sorry_for_goto;
                        }
                    }
                }
                sorry_for_goto: ;
            }
        }

        totalsize -= readsize;
        chunknum++;
    }
    return found; //size of pushed back values
}*/


NAMESPACE_END(RE)

//[[deprecated("May be moved to scanner.hh")]]
//bool find_pattern(uintptr_t *out, Cregion *region, const char *pattern, const char *mask) {
//    char buffer[0x1000];
//
//    uintptr_t len = strlen(mask);
//    uintptr_t chunksize = sizeof(buffer);
//    uintptr_t totalsize = region->size;
//    uintptr_t chunknum = 0;
//
//    while (totalsize) {
//        uintptr_t readsize = (totalsize < chunksize) ? totalsize : chunksize;
//        uintptr_t readaddr = region->address + (chunksize * chunknum);
//        bzero(buffer, chunksize);
//
//        if (this->read(readaddr, buffer, readsize)) {
//            for (uintptr_t b = 0; b < readsize; b++) {
//                uintptr_t matches = 0;
//
//                // если данные совпадают или пропустить
//                while (buffer[b + matches] == pattern[matches] || mask[matches] != 'x') {
//                    matches++;
//
//                    if (matches == len) {
//                        *out = readaddr + b;
//                        return true;
//                    }
//                }
//            }
//        }
//
//        totalsize -= readsize;
//        chunknum++;
//    }
//    return false;
//}