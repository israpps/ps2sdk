#include "part_driver.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sysmem.h>

#include <bdm.h>
#include "mbr_types.h"

#include "module_debug.h"

int partitions_sanity_check_mbr(struct block_device *bd, master_boot_record* pMbrBlock) {
    //All 4 MBR partitions should be considered valid ones to be mounted, even if some are unused.
    //At least one of them must be active.
    int valid = 0;
    int active = 0;
    
    for (int i = 0; i < 4; i++)
    {
        
        if (pMbrBlock->primary_partitions[i].partition_type != 0) {
            
            if((pMbrBlock->primary_partitions[i].first_lba == 0) || (pMbrBlock->primary_partitions[i].first_lba >= bd->sectorCount))
                return 0; //invalid
            
            active++;
        }
        
        valid++; //Considered at least a valid partition.
    }
    
    return (valid == 4) && (active > 0);
}

int part_connect_mbr(struct block_device *bd)
{
    master_boot_record* pMbrBlock = NULL;
    int rval = -1;
    int ret;
    int mountCount = 0;
    int partIndex;
    int valid_partitions;

    M_DEBUG("%s\n", __func__);
    
    // Filter out any block device where sectorOffset != 0, as this will be a block device for a file system partition and not
    // the raw device.
    if (bd->sectorOffset != 0)
        return rval;

    // Allocate memory for MBR partition sector.
    pMbrBlock = AllocSysMemory(ALLOC_FIRST, sizeof(master_boot_record), NULL);
    if (pMbrBlock == NULL)
    {
        // Failed to allocate memory for mbr block.
        M_DEBUG("Failed to allocate memory for MBR block\n");
        return rval;
    }

    // Read the MBR block from the block device.
    ret = bd->read(bd, 0, pMbrBlock, 1);
    if (ret < 0)
    {
        // Failed to read MBR block from the block device.
        M_DEBUG("Failed to read MBR sector from block device %d\n", ret);
        return rval;
    }

    // Check the MBR boot signature.
    if (pMbrBlock->boot_signature != MBR_BOOT_SIGNATURE)
    {
        // Boot signature is invalid, device is not valid MBR.
        M_DEBUG("MBR boot signature is invalid, device is not MBR\n");
        FreeSysMemory(pMbrBlock);
        return rval;
    }

    // Check the first primary partition to see if this is a GPT protective MBR.
    if (pMbrBlock->primary_partitions[0].partition_type == MBR_PART_TYPE_GPT_PROTECTIVE_MBR)
    {
        // We explicitly fail to connect GPT protective MBRs in order to let the GPT driver handle it.
        FreeSysMemory(pMbrBlock);
        return rval;
    }
    
    
    valid_partitions = partitions_sanity_check_mbr(bd, pMbrBlock);

    //Most likely a VBR
    if(valid_partitions == 0) {
        printf("MBR disk valid_partitions=%d \n", valid_partitions);
        FreeSysMemory(pMbrBlock);
        return -1;
    }

    printf("Found MBR disk\n");

    // Loop and parse the primary partition entries in the MBR block.
    for (int i = 0; i < 4; i++)
    {
        // Check if the partition is active, checking the status bit is not reliable so check if the sector_count is greater than zero instead.
        if (pMbrBlock->primary_partitions[i].sector_count == 0)
            continue;

        //Ignore partitions that are not active. 0 is empty partition_type.
        if (pMbrBlock->primary_partitions[i].partition_type == 0)
            continue;

        printf("Found partition type 0x%02x\n", pMbrBlock->primary_partitions[i].partition_type);
        
        // TODO: Filter out unsupported partition types.

        if ((partIndex = GetNextFreePartitionIndex()) == -1)
        {
            // No more free partition slots.
            printf("Can't mount partition, no more free partition slots!\n");
            continue;
        }

        // Create the pseudo block device for the partition.
        g_part[partIndex].bd              = bd;
        g_part_bd[partIndex].name         = bd->name;
        g_part_bd[partIndex].devNr        = bd->devNr;
        g_part_bd[partIndex].parNr        = i + 1;
        g_part_bd[partIndex].parId        = pMbrBlock->primary_partitions[i].partition_type;
        g_part_bd[partIndex].sectorSize   = bd->sectorSize;
        g_part_bd[partIndex].sectorOffset = bd->sectorOffset + (u64)pMbrBlock->primary_partitions[i].first_lba;
        g_part_bd[partIndex].sectorCount  = pMbrBlock->primary_partitions[i].sector_count;
        bdm_connect_bd(&g_part_bd[partIndex]);
        mountCount++;
    }

    // If one or more partitions were mounted then return success.
    rval = mountCount > 0 ? 0 : -1;

    FreeSysMemory(pMbrBlock);
    return rval;
}
