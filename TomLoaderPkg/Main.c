#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/DiskIo2.h>
#include <Protocol/BlockIo.h>

/// # MemoryMap
/// @brief メモリマップのデータを格納する配列
struct MemoryMap
{
    UINTN buffer_size;
    VOID *buffer;
    UINTN map_size;
    UINTN map_key;
    UINTN descriptor_size;
    UINT32 descriptor_version;
};

/// # GetMemoryMap
/// @param map 
/// @return EFI_STATUS
/// @brief メモリマップを取得する
/// # MemoryMapのデータ構造
/// ```
///                           / -> map.buffer
/// +                       +/
/// +-----------------------+ \---------------------------\ 
/// | EFI_MEMORY_DESCRIPTOR |  } -> map.descriptor_size    |
///.+-----------------------+  |                           |
/// +-----------------------+ /                            |
/// | EFI_MEMORY_DESCRIPTOR |                              } -> map.map_size
/// +-----------------------+                              |
/// +-----------------------+                              |
/// |          ...          |                              |
/// +-----------------------+                              |
/// +-----------------------+ ----------------------------/
/// +                       +
/// ```
EFI_STATUS GetMemoryMap(struct MemoryMap *map) {
    if (map->buffer == NULL){
        return EFI_BUFFER_TOO_SMALL;
    }

    map->map_size = map->buffer_size;
    // # EDK2のグローバル変数
    // global Boot Service: gBS
    // global Runtime Service : gRS
    return gBS->GetMemoryMap(
        &map->map_size,                        // 1: IN OUT UINTN *MemoryMapSize             # -> メモリマップ書き込み用のメモリ領域の大きさ
        (EFI_MEMORY_DESCRIPTOR*) map->buffer,  // 2: IN OUT EFI_MEMORY_DESCRIPTOR *MemoryMap # -> メモリマップ書き込み用のメモリ領域の先頭アドレス
                                               //    -- --- 
                                               //     \  \ 
                                               //      \--\--> (IN):メモリ領域の先頭ポインタを入力するという意味
                                               //          \--> (OUT):メモリマップが書き込まれるという意味
        &map->map_key,                         // 3: OUT UINTN *MapKey     # -> メモリマップの識別に使う値 後で`gBS->ExitBootService()`を呼び出すときに使う
        &map->descriptor_size,                 // 4: OUT UINTN *DescriptorSize # -> UEFIの実装によって値がまちまちなため`sizeof(EFI_MEMORY_DESCRIPTOR)`では不正確
        &map->descriptor_version               // 5: OUT UINT32 *DescriptorVersion -> メモリマップのバージョン番号
    );// 書き込みが成功したらEFI_SUCCESSを返却する
}

const CHAR16 *GetMemoryTypeUnicode(EFI_MEMORY_TYPE type){
    switch (type)
    {
    case EfiReservedMemoryType:return L"EfiReservedMemoryType";
    case EfiLoaderCode:return L"EfiLoaderCode";
    case EfiLoaderData:return L"EfiLoaderData";
    case EfiBootServicesCode:return L"EfiBootServicesCode";
    case EfiBootServicesData:return L"EfiBootServicesData";
    case EfiRuntimeServicesCode:return L"EfiRuntimeServicesCode";
    case EfiRuntimeServicesData:return L"EfiRuntimeServicesData";
    case EfiConventionalMemory:return L"EfiConventionalMemory";
    case EfiUnusableMemory:return L"EfiUnusableMemory";
    case EfiACPIReclaimMemory:return L"EfiACPIReclaimMemory";
    case EfiACPIMemoryNVS:return L"EfiACPIMemoryNVS";
    case EfiMemoryMappedIO:return L"EfiMemoryMappedIO";
    case EfiMemoryMappedIOPortSpace:return L"EfiMemoryMappedIOPortSpace";
    case EfiPalCode:return L"EfiPalCode";
    case EfiPersistentMemory:return L"EfiPersistentMemory";
    case EfiMaxMemoryType:return L"EfiMaxMemoryType";
    default:return L"InvalidMemoryType";
    }
}

EFI_STATUS SaveMemoryMap(struct MemoryMap *map,EFI_FILE_PROTOCOL *file){
    CHAR8 buf[256];
    UINTN len;

    CHAR8 *header = "Index, Type, Type(Name), PhysicalStart, NumberOfPages, Attribution\n";
    len = AsciiStrLen(header);
    file->Write(file, &len, header);

    Print(L"map -> buffer = %08lx, map -> map_size = %08lx\n", map -> buffer, map -> map_size);

    EFI_PHYSICAL_ADDRESS iter;
    int i;
    for (
        iter = (EFI_PHYSICAL_ADDRESS)map->buffer,i = 0;
        iter < (EFI_PHYSICAL_ADDRESS)map->buffer + map->map_size;
        iter += map -> descriptor_size, i++
    ){
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR*)iter;
        len = AsciiSPrint(
            buf, sizeof(buf),
            "%u, %x, %-ls, %08lx, %lx,%lx\n",
            i, desc->PhysicalStart, desc->NumberOfPages,
            desc->Attribute &0xffffflu
        );
        file->Write(file, &len, buf);
    }
    return EFI_SUCCESS;
}

EFI_STATUS OpenRootDir(EFI_HANDLE image_handle, EFI_FILE_PROTOCOL** root){
    EFI_LOADED_IMAGE_PROTOCOL* loaded_image;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs;

    gBS->OpenProtocol(
        image_handle,
        &gEfiLoadedImageProtocolGuid,
        (VOID**) &fs,
        image_handle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
    );

    gBS->OpenProtocol(
        loaded_image->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID**)&fs,
        image_handle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
    );

    fs->OpenVolume(fs, root);
    return EFI_SUCCESS;
}


EFI_STATUS EFIAPI UefiMain(
    EFI_HANDLE image_handle,
    EFI_SYSTEM_TABLE* system_table
){
    Print(L"Hello, TOMOS World!\n");
    CHAR8 memmap_buf[4096 * 4];
    struct MemoryMap memmap = {
                           // memmap structure definition
        sizeof(memmap_buf),// UINTN buffer_size;
        memmap_buf,        // VOID *buffer;
        0,                 // UINTN map_size;
        0,                 // UINTN map_key;
        0,                 // UINTN descriptor_size;
        0                  // UINT32 descriptor_version;
    };
    GetMemoryMap(&memmap);

    EFI_FILE_PROTOCOL* root_dir;
    OpenRootDir(image_handle, &root_dir);

    EFI_FILE_PROTOCOL*memmap_file;
    root_dir->Open(
        root_dir, &memmap_file, L"\\memmap",
        EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
        0
    );

    SaveMemoryMap(&memmap, memmap_file);
    memmap_file->Close(memmap_file);

    Print(L"All Done");

    while (1){
        return EFI_SUCCESS;
    }
}