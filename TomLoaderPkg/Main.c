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

