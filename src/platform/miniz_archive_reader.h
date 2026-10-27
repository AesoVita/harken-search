#pragma once
#include <cstddef>

// ABI declarations for the bundled miniz implementation already linked by the product.
extern "C" {
using mz_bool = unsigned char;
using mz_uint = unsigned int;
using mz_uint32 = unsigned int;
using mz_uint64 = unsigned long long;
using mz_alloc_func = void* (*)(void*, size_t);
using mz_free_func = void (*)(void*, void*);
using mz_realloc_func = void* (*)(void*, void*, size_t);
using mz_file_read_func = size_t (*)(void*, mz_uint64, void*, size_t);
using mz_file_write_func = size_t (*)(void*, mz_uint64, const void*, size_t);
using mz_file_needs_keepalive = mz_bool (*)(void*);
struct mz_zip_internal_state_tag;
using mz_zip_internal_state = mz_zip_internal_state_tag;
enum mz_zip_mode { MZ_ZIP_MODE_INVALID = 0, MZ_ZIP_MODE_READING = 1,
                   MZ_ZIP_MODE_WRITING = 2,
                   MZ_ZIP_MODE_WRITING_HAS_BEEN_FINALIZED = 3 };
enum mz_zip_type { MZ_ZIP_TYPE_INVALID = 0, MZ_ZIP_TYPE_USER,
                   MZ_ZIP_TYPE_MEMORY, MZ_ZIP_TYPE_HEAP,
                   MZ_ZIP_TYPE_FILE, MZ_ZIP_TYPE_CFILE,
                   MZ_ZIP_TOTAL_TYPES };
enum mz_zip_error { MZ_ZIP_NO_ERROR = 0 };
struct mz_zip_archive {
    mz_uint64 m_archive_size;
    mz_uint64 m_central_directory_file_ofs;
    mz_uint32 m_total_files;
    mz_zip_mode m_zip_mode;
    mz_zip_type m_zip_type;
    mz_zip_error m_last_error;
    mz_uint64 m_file_offset_alignment;
    mz_alloc_func m_pAlloc;
    mz_free_func m_pFree;
    mz_realloc_func m_pRealloc;
    void* m_pAlloc_opaque;
    mz_file_read_func m_pRead;
    mz_file_write_func m_pWrite;
    mz_file_needs_keepalive m_pNeeds_keepalive;
    void* m_pIO_opaque;
    mz_zip_internal_state* m_pState;
};
mz_bool mz_zip_reader_init(mz_zip_archive*, mz_uint64, mz_uint);
mz_bool mz_zip_reader_extract_file_to_callback(
    mz_zip_archive*, const char*, mz_file_write_func, void*, mz_uint);
int mz_zip_reader_locate_file(mz_zip_archive*, const char*, const char*, mz_uint);
mz_bool mz_zip_reader_end(mz_zip_archive*);
}
