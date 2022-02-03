
/*
** Copyright (c) 2022 LunarG, Inc.
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and associated documentation files (the "Software"),
** to deal in the Software without restriction, including without limitation
** the rights to use, copy, modify, merge, publish, distribute, sublicense,
** and/or sell copies of the Software, and to permit persons to whom the
** Software is furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
** FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
** DEALINGS IN THE SOFTWARE.
*/

#include "graphics/vulkan_device_address_map.h"

#include "util/logging.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(graphics)

constexpr uint64_t kNullAddress = 0;

void VkDeviceAddressMap::Add(format::HandleId resource_id,
                             VkDeviceAddress  old_start_address,
                             VkDeviceSize     old_size,
                             VkDeviceAddress  new_start_address)
{
    if ((resource_id != format::kNullHandleId) && (old_start_address != kNullAddress) &&
        (new_start_address != kNullAddress))
    {
        auto& aliased_resource_list     = dev_addr_map_[old_start_address];
        auto& resource_info             = aliased_resource_list[resource_id];
        resource_info.old_end_address   = old_start_address + old_size;
        resource_info.new_start_address = new_start_address;
    }
}

void VkDeviceAddressMap::Remove(format::HandleId resource_id, VkDeviceAddress old_start_address)
{
    if ((resource_id != format::kNullHandleId) && (old_start_address != kNullAddress))
    {
        auto entry = dev_addr_map_.find(old_start_address);
        if (entry != dev_addr_map_.end())
        {
            entry->second.erase(resource_id);
            if (entry->second.empty())
            {
                dev_addr_map_.erase(entry);
            }
        }
    }
}

VkDeviceAddress VkDeviceAddressMap::Map(VkDeviceAddress address, format::HandleId* resource_id, bool* found) const
{
    bool local_found = false;

    if (address != kNullAddress)
    {
        auto addr_entry = dev_addr_map_.lower_bound(address);
        if (addr_entry != dev_addr_map_.end())
        {
            // Check for a match in the aliased resource list.
            local_found = FindMatch(addr_entry->second, addr_entry->first, address, resource_id);

            // The addresses did not fall within the address range of the resource(s) at the start address returned
            // by the lower_bound search.  These resources may be aliased with a larger resource that contains them.
            // Check for an aliased resource with a smaller start address.  The entries in the GPU VA map are sorted
            // in descending order to get a <= behavior from lower_bound, so the next smallest address is obtained
            // by incrementing the iterator.

            // NOTE: This turns the O(logn) search into a O(n) search when an entry is not found in the map.  If this
            // becomes an issue, it is possible that the add and remove operations be changed to merge all aliased
            // resources into a single entry keyed by the smallest start address of all of the aliased resources.  On
            // remove, if none of the remaining aliased resources share the start address that is used for the key to
            // the entry, the entry would need to be removed and re-added with the smallest address of the remaining
            // aliased resources as the key.
            while (!local_found && ++addr_entry != dev_addr_map_.end())
            {
                local_found = FindMatch(addr_entry->second, addr_entry->first, address, resource_id);
            }
        }

        if (!local_found && (found == nullptr))
        {
            GFXRECON_LOG_WARNING("No matching replay VkDeviceAddress found for capture VkDeviceAddress 0x%" PRIx64,
                                 address);
        }
    }

    if (found != nullptr)
    {
        (*found) = local_found;
    }

    return address;
}

bool VkDeviceAddressMap::FindMatch(const AliasedResourceVaInfo& resource_info,
                                   VkDeviceAddress              old_start_address,
                                   VkDeviceAddress&             address,
                                   format::HandleId*            resource_id) const
{
    // Check for a match in the aliased resource list.
    for (const auto& resource_entry : resource_info)
    {
        const auto& info = resource_entry.second;

        if (address < info.old_end_address)
        {
            address = info.new_start_address + (address - old_start_address);
            if (resource_id != nullptr)
            {
                *resource_id = resource_entry.first;
            }
            return true;
        }
    }

    return false;
}

GFXRECON_END_NAMESPACE(graphics)
GFXRECON_END_NAMESPACE(gfxrecon)
