// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"


//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// ProcessAddressSpace::ProcessAddressSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

ProcessAddressSpace::ProcessAddressSpace(OpenFile *executable, char* f)
{
    // NoffHeader noffH;
    unsigned int i, size;
    unsigned vpn, offset;
    TranslationEntry *entry;
    unsigned int pageFrame;
    filename = new char[1024];
    for (int i = 0; i < 1024; ++i)
    {
        filename[i] = f[i];
        if (f[i] == '\n' || f[i] == '\0')
            break;        
    }
    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numVirtualPages = divRoundUp(size, PageSize);
    size = numVirtualPages * PageSize;

    ASSERT(numVirtualPages+numPagesAllocated <= NumPhysPages);		// check we're not trying
										// to run anything too big --
										// at least until we have
										// virtual memory

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
					numVirtualPages, size);
// first, set up the translation 
    KernelPageTable = new TranslationEntry[numVirtualPages];
    for (i = 0; i < numVirtualPages; i++) {
	KernelPageTable[i].virtualPage = i;
	KernelPageTable[i].physicalPage = i+numPagesAllocated;
	KernelPageTable[i].valid = FALSE;
	KernelPageTable[i].use = FALSE;
	KernelPageTable[i].dirty = FALSE;
	KernelPageTable[i].readOnly = FALSE;  // if the code segment was entirely on 
					// a separate page, we could set its 
					// pages to be read-only
    KernelPageTable[i].shared = FALSE;
    }
// zero out the entire address space, to zero the unitialized data segment 
// and the stack segment
    bzero(&machine->mainMemory[numPagesAllocated*PageSize], size);
 
 // We dont need this for demand paging
    // numPagesAllocated += numVirtualPages;

// // then, copy in the code and data segments into memory
//     if (noffH.code.size > 0) {
//         DEBUG('a', "Initializing code segment, at 0x%x, size %d\n",
// 			noffH.code.virtualAddr, noffH.code.size);
//         vpn = noffH.code.virtualAddr/PageSize;
//         offset = noffH.code.virtualAddr%PageSize;
//         entry = &KernelPageTable[vpn];
//         pageFrame = entry->physicalPage;
//         executable->ReadAt(&(machine->mainMemory[pageFrame * PageSize + offset]),
// 			noffH.code.size, noffH.code.inFileAddr);
//     }
//     if (noffH.initData.size > 0) {
//         DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
// 			noffH.initData.virtualAddr, noffH.initData.size);
//         vpn = noffH.initData.virtualAddr/PageSize;
//         offset = noffH.initData.virtualAddr%PageSize;
//         entry = &KernelPageTable[vpn];
//         pageFrame = entry->physicalPage;
//         executable->ReadAt(&(machine->mainMemory[pageFrame * PageSize + offset]),
// 			noffH.initData.size, noffH.initData.inFileAddr);
//     }

}

//----------------------------------------------------------------------
// ProcessAddressSpace::ProcessAddressSpace (ProcessAddressSpace*) is called by a forked thread.
//      We need to duplicate the address space of the parent.
//----------------------------------------------------------------------

ProcessAddressSpace::ProcessAddressSpace(ProcessAddressSpace *parentSpace)
{
    numVirtualPages = parentSpace->GetNumPages();
    unsigned i, size = numVirtualPages * PageSize;

    ASSERT(numVirtualPages+numPagesAllocated <= NumPhysPages);                // check we're not trying
                                                                                // to run anything too big --
                                                                                // at least until we have
                                                                                // virtual memory

    DEBUG('a', "Initializing address space, num pages %d, size %d\n",
                                        numVirtualPages, size);
    // first, set up the translation
    TranslationEntry* parentPageTable = parentSpace->GetPageTable();
    KernelPageTable = new TranslationEntry[numVirtualPages];
    filename = new char[1024];
    noffH = parentSpace->noffH;
    for (int i = 0; i < 1024; ++i)
    {
        filename[i] = parentSpace->filename[i];
        if (filename[i] == '\0' || filename[i] == '\n')
            break;
    }
    int pagesAssigned = 0;

    for (i = 0; i < numVirtualPages; i++) {
        int temp = i;
        LRU_list->Append(*temp);
        KernelPageTable[i].virtualPage = i;
        if (parentPageTable[i].shared) {
            KernelPageTable[i].physicalPage = parentPageTable[i].physicalPage;    
        } else {
            if (parentPageTable[i].valid == TRUE){
                KernelPageTable[i].physicalPage = numPagesAllocated + pagesAssigned;
                pagesAssigned += 1;
            }
            else
                KernelPageTable[i].physicalPage = 0;
        }
        KernelPageTable[i].valid = parentPageTable[i].valid;
        KernelPageTable[i].use = parentPageTable[i].use;
        KernelPageTable[i].dirty = parentPageTable[i].dirty;
        KernelPageTable[i].readOnly = parentPageTable[i].readOnly;  	// if the code segment was entirely on
                                        			// a separate page, we could set its
                                        			// pages to be read-only
        KernelPageTable[i].shared = parentPageTable[i].shared;
    }

    // Copy the contents
    // unsigned startAddrParent = parentPageTable[0].physicalPage*PageSize;
    // unsigned startAddrChild = numPagesAllocated*PageSize;
    // unsigned copySize = numVirtualPages * pagesAssigned;
    // for (i=0; i<copySize; i++) {
    //    machine->mainMemory[startAddrChild+i] = machine->mainMemory[startAddrParent+i];
    // }
    int j;
    for (i = 0; i < numVirtualPages; i++)
    {
        if (KernelPageTable[i].shared == FALSE && KernelPageTable[i].valid == TRUE)
        {
            for (j = 0; j < PageSize; ++j)
            {
                machine->mainMemory[KernelPageTable[i].physicalPage*PageSize+j] = machine->mainMemory[parentPageTable[i].physicalPage*PageSize+j];
            }
        }
    }

    numPagesAllocated += pagesAssigned;
    stats->totalPageFaults++;
}

//----------------------------------------------------------------------
// ProcessAddressSpace::~ProcessAddressSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

ProcessAddressSpace::~ProcessAddressSpace()
{
   delete KernelPageTable;
}

//----------------------------------------------------------------------
// ProcessAddressSpace::InitUserModeCPURegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
ProcessAddressSpace::InitUserModeCPURegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numVirtualPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numVirtualPages * PageSize - 16);
}

//----------------------------------------------------------------------
// ProcessAddressSpace::SaveContextOnSwitch
// 	On a context switch, save any machine state, specificFork
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void ProcessAddressSpace::SaveContextOnSwitch() 
{}

//----------------------------------------------------------------------
// ProcessAddressSpace::RestoreContextOnSwitch
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void ProcessAddressSpace::RestoreContextOnSwitch() 
{
    machine->KernelPageTable = KernelPageTable;
    machine->KernelPageTableSize = numVirtualPages;
}

unsigned
ProcessAddressSpace::GetNumPages()
{
   return numVirtualPages;
}

TranslationEntry*
ProcessAddressSpace::GetPageTable()
{
   return KernelPageTable;
}

unsigned
ProcessAddressSpace::sharedMemory(int numSharedPages)
{
    TranslationEntry* parentPageTable = GetPageTable();
    TranslationEntry* KernelPageTable1 = new TranslationEntry[numVirtualPages+numSharedPages];
    int i;
    for (i = 0; i < numVirtualPages; i++) {
        KernelPageTable1[i].virtualPage = i;
        KernelPageTable1[i].physicalPage = parentPageTable[i].physicalPage;
        KernelPageTable1[i].valid = parentPageTable[i].valid;
        KernelPageTable1[i].use = parentPageTable[i].use;
        KernelPageTable1[i].dirty = parentPageTable[i].dirty;
        KernelPageTable1[i].readOnly = parentPageTable[i].readOnly;  	// if the code segment was entirely on
                                        			// a separate page, we could set its
                                        			// pages to be read-only
        KernelPageTable1[i].shared = parentPageTable[i].shared;
    }
    for (i = numVirtualPages; i < numVirtualPages+numSharedPages; ++i)
    {
        KernelPageTable1[i].virtualPage = i;
        KernelPageTable1[i].physicalPage = numPagesAllocated+i-numVirtualPages;
        KernelPageTable1[i].valid = TRUE;
	    KernelPageTable1[i].use = FALSE;
	    KernelPageTable1[i].dirty = FALSE;
	    KernelPageTable1[i].readOnly = FALSE;  // if the code segment was entirely on 
	    				// a separate page, we could set its 
	    				// pages to be read-only
        KernelPageTable1[i].shared = TRUE;
        stats->totalPageFaults++;
    }
    numPagesAllocated += numSharedPages;
    List* LRU_temp = LRU_list->first;
    List* LRU_temp_2;
    while(LRU_temp != NULL){
        i = LRU_temp->item;     
        if(KernelPageTable[i].shared == TRUE){
            if(LRU_list->first == LRU_temp)LRU_list->first = LRU_temp->next;
            LRU_temp->prev->next = LRU_temp->next;
            LRU_temp->next->prev = LRU_temp->prev;
            LRU_temp_2 = LRU_temp;
            LRU_temp = LRU_temp->next;
            delete LRU_temp_2; 
        }
    }

    KernelPageTable = KernelPageTable1;
    unsigned virtualAddressStarting = numVirtualPages*PageSize;
    numVirtualPages += numSharedPages;
    printf("numSharedPages: %d\n", numSharedPages);
    printf("numVirtualPages %d\n", numVirtualPages);
    RestoreContextOnSwitch();
    delete parentPageTable;
    return virtualAddressStarting;
}

unsigned
ProcessAddressSpace::GetPhysicalPage(unsigned vpn, int flag)
{
    // Assuming that we have to allocate a new page everytime the page is accessed. Change this suitably for different replacement algorithms
    int RANDOM = 1;
    int FIFO = 2;
    int LRU = 3;
    int LRU_CLOCK = 4;
    if(flag == RANDOM){

    }else if(flag == FIFO){

    }else if(flag == LRU){
        return currentThread->LRU_list->last->item;
    }else if(flag == LRU_CLOCK){

    }

}

void
ProcessAddressSpace::CopyPageData(unsigned vpn, bool useNoffH)
{
    if (useNoffH)
    {
        unsigned startCopyAddr = vpn*PageSize, ppn = KernelPageTable[vpn].physicalPage;
        OpenFile *executable = fileSystem->Open(filename);
        printf("noffH.code.virtualAddr: %d\n", noffH.code.virtualAddr);
        executable->ReadAt(&(machine->mainMemory[ppn * PageSize]), PageSize, noffH.code.inFileAddr + startCopyAddr - noffH.code.virtualAddr);
        delete executable;
        currentThread->SortedInsertInWaitQueue(1000+stats->totalTicks);
    }
}

void
ProcessAddressSpace::PageFaultHandler(unsigned vaddr, int flag)
{
    unsigned vpn = vaddr/PageSize;
    unsigned ppn = GetPhysicalPage(vpn, flag);
    // printf("ppn:%d\n",ppn);
    KernelPageTable[vpn].virtualPage = vpn;
	KernelPageTable[vpn].physicalPage = ppn;
	KernelPageTable[vpn].valid = TRUE;
	KernelPageTable[vpn].use = FALSE;
	KernelPageTable[vpn].dirty = FALSE;
	KernelPageTable[vpn].readOnly = FALSE;  // if the code segment was entirely on 
					// a separate page, we could set its 
					// pages to be read-only
    KernelPageTable[vpn].shared = FALSE;
    DEBUG('a', "Page fault detected for %d virtual address vpn: %d. Loading Physical page into memory\n", vaddr, vpn);    
    CopyPageData(vpn, true);
}