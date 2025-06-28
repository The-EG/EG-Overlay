// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

//! Compressed file access
pub mod lua;

use std::rc::Rc;

use std::collections::HashMap;

use std::fs::File;
use std::io::{Seek, Read};
use std::io::SeekFrom;
use std::io::ErrorKind;

/// A compressed (zip) file
pub struct ZipFile {
    //path: String,
    file: File,

    cd_offset: u64,
    cd_size: u64,

    central_directory: HashMap<String, ZipCentralDirectoryHeader>,
}

/// A record in the central directory
///
/// This represents a file in the archive and contains information needed
/// to find and uncompress the file.
#[derive(Default)]
struct ZipCentralDirectoryHeader {
    version_by: u16,
    version_extract: u16,
    flags: u16,
    compression: u16,
    file_mod_time: u16,
    file_mod_date: u16,
    file_crc: u32,
    file_compressed_size: u32,
    file_uncompressed_size: u32,
    file_name_len: u16,
    extra_field_len: u16,
    comment_len: u16,
    disk_num: u16,
    internal_attrs: u16,
    external_attrs: u32,
    file_offset: u32,
    file_name: String,
    extra_field: Vec<u8>,
    file_comment: String,
}

/// Opens a compressed archive file.
pub fn open_zip(path: &str) -> std::io::Result<Rc<ZipFile>> {
    let f = std::fs::OpenOptions::new().read(true).open(path)?;

    let mut zip = ZipFile {
        //path: String::from(path),
        file: f,

        cd_offset: 0,
        cd_size: 0,

        central_directory: HashMap::new(),
    };
    
    zip.find_central_directory()?;
    zip.load_central_directory()?;

    Ok(Rc::new(zip))
}

impl ZipFile {
    fn find_central_directory(&mut self) -> std::io::Result<()> {
        let mut eocd_start: i64 = -22;

        let file_size = self.file.seek(SeekFrom::End(eocd_start))? as i64 - eocd_start;

        let mut eocd_sig = [0u8; 4];
        while (eocd_start * -1) < (file_size - 22) {
            self.file.read_exact(&mut eocd_sig)?;
            let sig = u32::from_le_bytes(eocd_sig);

            // this is the end of central directory record signature
            if sig == 0x06054b50 { break; }

            eocd_start -= 1;
            self.file.seek(SeekFrom::End(eocd_start - 4))?; // - 4 because we read 4 bytes above
        }

        if (eocd_start * -1) >= (file_size - 22) {
            return Err(std::io::Error::new(ErrorKind::Other, "could not find end of central directory record"));
        }

        // this should be 0, unless someone tries to use part of an archive
        // originally stored on floppy disks
        let _cd_disk_num = self.read_u16()?;

        // these might indicate a ZIP64 file if they are -1
        let _cd_start_disck = self.read_u16()?;
        let _cd_disk_records = self.read_u16()?;

        let _cd_records = self.read_u16()?;

        self.cd_size = self.read_u32()? as u64;
        self.cd_offset = self.read_u32()? as u64;

        if self.cd_size==0xFFFFFFFF || self.cd_offset==0xFFFFFFFF {
            // this is a ZIP64
            return Err(std::io::Error::new(ErrorKind::Other, "zip64 not implemented"));
        }

        Ok(())
    }

    fn load_central_directory(&mut self) -> std::io::Result<()> {
        self.file.seek(SeekFrom::Start(self.cd_offset))?;

        let mut cd_read_size = 0;
        while cd_read_size < self.cd_size {
            let cd = self.read_central_directory_header()?;
            cd_read_size += cd.size() as u64;
            self.central_directory.insert(cd.file_name.to_lowercase(),cd);
        }

        Ok(())
    }

    fn read_central_directory_header(&mut self) -> std::io::Result<ZipCentralDirectoryHeader> {
        let mut hdr = ZipCentralDirectoryHeader::default();

        if self.read_u32()? != 0x02014b50 {
            return Err(std::io::Error::new(ErrorKind::Other, "expected central directory header signature"));
        }

        hdr.version_by             = self.read_u16()?;
        hdr.version_extract        = self.read_u16()?;
        hdr.flags                  = self.read_u16()?;
        hdr.compression            = self.read_u16()?;
        hdr.file_mod_time          = self.read_u16()?;
        hdr.file_mod_date          = self.read_u16()?;
        hdr.file_crc               = self.read_u32()?;
        hdr.file_compressed_size   = self.read_u32()?;
        hdr.file_uncompressed_size = self.read_u32()?;
        hdr.file_name_len          = self.read_u16()?;
        hdr.extra_field_len        = self.read_u16()?;
        hdr.comment_len            = self.read_u16()?;
        hdr.disk_num               = self.read_u16()?;
        hdr.internal_attrs         = self.read_u16()?;
        hdr.external_attrs         = self.read_u32()?;
        hdr.file_offset            = self.read_u32()?;

        hdr.file_name    = self.read_string(hdr.file_name_len as usize)?;
        hdr.extra_field  = self.read_bytes(hdr.extra_field_len as usize)?;
        hdr.file_comment = self.read_string(hdr.comment_len as usize)?;

        Ok(hdr)
    }

    fn file_content(&mut self, path: &str) -> std::io::Result<Vec<u8>> {
        let pathlower = path.to_lowercase().replace("\\","/");

        if !self.central_directory.contains_key(&pathlower) {
            return Err(std::io::Error::from(ErrorKind::NotFound));
        }

        let cd = self.central_directory.get(&pathlower).unwrap();

        self.file.seek(SeekFrom::Start(cd.file_offset as u64))?;

        if self.read_u32()? != 0x04034b50 {
            return Err(std::io::Error::new(ErrorKind::Other, "didn't find local file header"));
        }

        //let _version_extract  = self.read_u16()?;
        //let _flags            = self.read_u16()?;
        self.file.seek(SeekFrom::Current(4))?;

        let compression = self.read_u16()?;

        //let _last_mod_time    = self.read_u16()?;
        //let _last_mod_date    = self.read_u16()?;
        self.file.seek(SeekFrom::Current(4))?;

        let crc32 = self.read_u32()?;

        let compressed_size   = self.read_u32()?;
        let uncompressed_size = self.read_u32()?;
        //self.file.seek(SeekFrom::Current(8))?;

        let file_name_len     = self.read_u16()?;
        let extra_field_len   = self.read_u16()?;

        self.file.seek(SeekFrom::Current(file_name_len as i64 + extra_field_len as i64))?;

        let compressed_data = self.read_bytes(compressed_size as usize)?;

        if compression == 0 { return Ok(compressed_data); }

        if compression != 8 {
            return Err(std::io::Error::new(ErrorKind::Other, "compression method not implemented"));
        }

        let mut strm = zlib::z_stream::default();

        if unsafe { zlib::inflateInit2_(
            &mut strm,
            -15, // raw compression, no zlib headers
            c"1.3".as_ptr(),
            std::mem::size_of::<zlib::z_stream>() as i32
        )} != 0 {
            return Err(std::io::Error::new(ErrorKind::Other, "couldn't initialize zlib"));
        }

        let mut uncompressed_data = vec![0u8; uncompressed_size as usize];

        strm.avail_out = uncompressed_size;
        strm.next_out = uncompressed_data.as_mut_ptr();
        strm.avail_in = compressed_size;
        strm.next_in = compressed_data.as_ptr();

        let r = unsafe { zlib::inflate(&mut strm, zlib::Z_FINISH) };
            
        unsafe { zlib::inflateEnd(&mut strm) };

        if r != zlib::Z_STREAM_END {
            return Err(std::io::Error::new(ErrorKind::Other, "expected stream end"));
        }

        let mut data_crc = unsafe { zlib::crc32(0, std::ptr::null(), 0) };
        
        data_crc = unsafe { zlib::crc32(data_crc, uncompressed_data.as_ptr(), uncompressed_data.len() as u32) };

        if data_crc != crc32 {
            return Err(std::io::Error::new(ErrorKind::Other, "crc mismatch"));
        }

        Ok(uncompressed_data)
    }

    fn read_u16(&mut self) -> std::io::Result<u16> {
        let mut d = [0u8;2];
        self.file.read_exact(&mut d)?;

        Ok(u16::from_le_bytes(d))
    }

    fn read_u32(&mut self) -> std::io::Result<u32> {
        let mut d = [0u8;4];
        self.file.read_exact(&mut d)?;

        Ok(u32::from_le_bytes(d))
    }

    fn read_bytes(&mut self, count: usize) -> std::io::Result<Vec<u8>> {
        let mut bytes = vec![0u8; count];
        self.file.read_exact(bytes.as_mut_slice())?;

        Ok(bytes)
    }

    fn read_string(&mut self, count: usize) -> std::io::Result<String> {
        let bytes = self.read_bytes(count)?;

        Ok(String::from_utf8_lossy(&bytes).into_owned())
    }
}

impl ZipCentralDirectoryHeader {
    fn size(&self) -> usize {
        /*
        4 signature
        2 version_by
        2 version_extract
        2 flags
        2 compression
        2 file_mod_time
        2 file_mod_date
        4 file_crc
        4 compressed size
        4 uncompressed size
        2 file name len
        2 extra field len
        2 comment len
        2 disk num
        2 internal attrs
        4 external attrs
        4 file offset
        */
        46 + self.file_name_len as usize + self.extra_field_len as usize+ self.comment_len as usize
    }
}

mod zlib {
    use std::ffi::{c_int, c_uint, c_ulong, c_char, c_void};

    pub const Z_FINISH: c_int = 4;
    pub const Z_STREAM_END: c_int = 1;

    #[repr(C)]
    pub struct z_stream {
        pub next_in: *const u8,
        pub avail_in: c_uint,
        pub total_in: c_ulong,

        pub next_out: *mut u8,
        pub avail_out: c_uint,
        pub total_out: c_ulong,

        pub msg: *const c_char,
        state: *mut c_void,

        zalloc: *mut c_void,
        zfree: *mut c_void,
        voidpf: *mut c_void,
        
        data_type: c_int,

        pub adler: c_ulong,
        reserved: c_ulong,
    }

    impl Default for z_stream {
        fn default() -> Self {
            z_stream {
                next_in: std::ptr::null(),
                avail_in: 0,
                total_in: 0,

                next_out: std::ptr::null_mut(),
                avail_out: 0,
                total_out: 0,

                msg: std::ptr::null(),
                state: std::ptr::null_mut(),

                zalloc: std::ptr::null_mut(),
                zfree: std::ptr::null_mut(),
                voidpf: std::ptr::null_mut(),

                data_type: 0,

                adler: 0,
                reserved: 0,
            }
        }
    }

    unsafe extern "C" {
        pub fn inflateInit2_(
            strm: *mut z_stream,
            windowBits: c_int,
            version: *const c_char,
            stream_size: c_int
        ) -> c_int;

        pub fn inflate(strm: *mut z_stream, flush: c_int) -> c_int;
        pub fn inflateEnd(strm: *mut z_stream) -> c_int;

        pub fn crc32(crc: c_ulong, buf: *const u8, len: c_uint) -> c_ulong;
    }
}
