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
/// @brief メモリマップを取得する
/// @param map 
/// @return EFI_STATUS
EFI_STATUS GetMemoryMap(struct MemoryMap *map) {
    if (map->buffer == NULL){
        return EFI_BUFFER_TOO_SMALL;
    }

    map->map_size = map->buffer_size;
    return gBS->GetMemoryMap(
        &map->map_size,
        (EFI_MEMORY_DESCRIPTOR*) map->buffer,
        &map->map_key,
        &map->descriptor_size,
        &map->descriptor_version
    );
}


