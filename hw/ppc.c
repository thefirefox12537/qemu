/*
 * QEMU generic PowerPC hardware System Emulator
 * 
 * Copyright (c) 2003-2007 Jocelyn Mayer
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "vl.h"
#include "m48t59.h"

//#define PPC_DEBUG_IRQ
//#define PPC_DEBUG_TB

extern FILE *logfile;
extern int loglevel;

void ppc_set_irq (CPUState *env, int n_IRQ, int level)
{
    if (level) {
        env->pending_interrupts |= 1 << n_IRQ;
        cpu_interrupt(env, CPU_INTERRUPT_HARD);
    } else {
        env->pending_interrupts &= ~(1 << n_IRQ);
        if (env->pending_interrupts == 0)
            cpu_reset_interrupt(env, CPU_INTERRUPT_HARD);
    }
#if defined(PPC_DEBUG_IRQ)
    if (loglevel & CPU_LOG_INT) {
        fprintf(logfile, "%s: %p n_IRQ %d level %d => pending %08x req %08x\n",
                __func__, env, n_IRQ, level,
                env->pending_interrupts, env->interrupt_request);
    }
#endif
}

/* PowerPC 6xx / 7xx internal IRQ controller */
static void ppc6xx_set_irq (void *opaque, int pin, int level)
{
    CPUState *env = opaque;
    int cur_level;

#if defined(PPC_DEBUG_IRQ)
    if (loglevel & CPU_LOG_INT) {
        fprintf(logfile, "%s: env %p pin %d level %d\n", __func__,
                env, pin, level);
    }
#endif
    cur_level = (env->irq_input_state >> pin) & 1;
    /* Don't generate spurious events */
    if ((cur_level == 1 && level == 0) || (cur_level == 0 && level != 0)) {
        switch (pin) {
        case PPC6xx_INPUT_INT:
            /* Level sensitive - active high */
#if defined(PPC_DEBUG_IRQ)
            if (loglevel & CPU_LOG_INT) {
                fprintf(logfile, "%s: set the external IRQ state to %d\n",
                        __func__, level);
            }
#endif
            ppc_set_irq(env, PPC_INTERRUPT_EXT, level);
            break;
        case PPC6xx_INPUT_SMI:
            /* Level sensitive - active high */
#if defined(PPC_DEBUG_IRQ)
            if (loglevel & CPU_LOG_INT) {
                fprintf(logfile, "%s: set the SMI IRQ state to %d\n",
                        __func__, level);
            }
#endif
            ppc_set_irq(env, PPC_INTERRUPT_SMI, level);
            break;
        case PPC6xx_INPUT_MCP:
            /* Negative edge sensitive */
            /* XXX: TODO: actual reaction may depends on HID0 status
             *            603/604/740/750: check HID0[EMCP]
             */
            if (cur_level == 1 && level == 0) {
#if defined(PPC_DEBUG_IRQ)
                if (loglevel & CPU_LOG_INT) {
                    fprintf(logfile, "%s: raise machine check state\n",
                            __func__);
                }
#endif
                ppc_set_irq(env, PPC_INTERRUPT_MCK, 1);
            }
            break;
        case PPC6xx_INPUT_CKSTP_IN:
            /* Level sensitive - active low */
            /* XXX: TODO: relay the signal to CKSTP_OUT pin */
            if (level) {
#if defined(PPC_DEBUG_IRQ)
                if (loglevel & CPU_LOG_INT) {
                    fprintf(logfile, "%s: stop the CPU\n", __func__);
                }
#endif
                env->halted = 1;
            } else {
#if defined(PPC_DEBUG_IRQ)
                if (loglevel & CPU_LOG_INT) {
                    fprintf(logfile, "%s: restart the CPU\n", __func__);
                }
#endif
                env->halted = 0;
            }
            break;
        case PPC6xx_INPUT_HRESET:
            /* Level sensitive - active low */
            if (level) {
#if 0 // XXX: TOFIX
#if defined(PPC_DEBUG_IRQ)
                if (loglevel & CPU_LOG_INT) {
                    fprintf(logfile, "%s: reset the CPU\n", __func__);
                }
#endif
                cpu_reset(env);
#endif
            }
            break;
        case PPC6xx_INPUT_SRESET:
#if defined(PPC_DEBUG_IRQ)
            if (loglevel & CPU_LOG_INT) {
                fprintf(logfile, "%s: set the RESET IRQ state to %d\n",
                        __func__, level);
            }
#endif
            ppc_set_irq(env, PPC_INTERRUPT_RESET, level);
            break;
        default:
            /* Unknown pin - do nothing */
#if defined(PPC_DEBUG_IRQ)
            if (loglevel & CPU_LOG_INT) {
                fprintf(logfile, "%s: unknown IRQ pin %d\n", __func__, pin);
            }
#endif
            return;
        }
        if (level)
            env->irq_input_state |= 1 << pin;
        else
            env->irq_input_state &= ~(1 << pin);
    }
}

void ppc6xx_irq_init (CPUState *env)
{
    env->irq_inputs = (void **)qemu_allocate_irqs(&ppc6xx_set_irq, env, 6);
}

/* PowerPC 970 internal IRQ controller */
static void ppc970_set_irq (void *opaque, int pin, int level)
{
    CPUState *env = opaque;
    int cur_level;

#if defined(PPC_DEBUG_IRQ)
    if (loglevel & CPU_LOG_INT) {
        fprintf(logfile, "%s: env %p pin %d level %d\n", __func__,
                env, pin, level);
    }
#endif
    cur_level = (env->irq_input_state >> pin) & 1;
    /* Don't generate spurious events */
    if ((cur_level == 1 && level == 0) || (cur_level == 0 && level != 0)) {
        switch (pin) {
        case PPC970_INPUT_INT:
            /* Level sensitive - active high */
#if defined(PPC_DEBUG_IRQ)
            if (loglevel & CPU_LOG_INT) {
                fprintf(logfile, "%s: set the external IRQ state to %d\n",
                        __func__, level);
            }
#endif
            ppc_set_irq(env, PPC_INTERRUPT_EXT, level);
            break;
        case PPC970_INPUT_THINT:
            /* Level sensitive - active high */
#if defined(PPC_DEBUG_IRQ)
            if (loglevel & CPU_LOG_INT) {
                fprintf(logfile, "%s: set the SMI IRQ state to %d\n", __func__,
                        level);
            }
#endif
            ppc_set_irq(env, PPC_INTERRUPT_THERM, level);
            break;
        case PPC970_INPUT_MCP:
            /* Negative edge sensitive */
            /* XXX: TODO: actual reaction may depends on HID0 status
             *            603/604/740/750: check HID0[EMCP]
             */
            if (cur_level == 1 && level == 0) {
#if defined(PPC_DEBUG_IRQ)
                if (loglevel & CPU_LOG_INT) {
                    fprintf(logfile, "%s: raise machine check state\n",
                            __func__);
                }
#endif
                ppc_set_irq(env, PPC_INTERRUPT_MCK, 1);
            }
            break;
        case PPC970_INPUT_CKSTP:
            /* Level sensitive - active low */
            /* XXX: TODO: relay the signal to CKSTP_OUT pin */
            if (level) {
#if defined(PPC_DEBUG_IRQ)
                if (loglevel & CPU_LOG_INT) {
                    fprintf(logfile, "%s: stop the CPU\n", __func__);
                }
#endif
                env->halted = 1;
            } else {
#if defined(PPC_DEBUG_IRQ)
                if (loglevel & CPU_LOG_INT) {
                    fprintf(logfile, "%s: restart the CPU\n", __func__);
                }
#endif
                env->halted = 0;
            }
            break;
        case PPC970_INPUT_HRESET:
            /* Level sensitive - active low */
            if (level) {
#if 0 // XXX: TOFIX
#if defined(PPC_DEBUG_IRQ)
                if (loglevel & CPU_LOG_INT) {
                    fprintf(logfile, "%s: reset the CPU\n", __func__);
                }
#endif
                cpu_reset(env);
#endif
            }
            break;
        case PPC970_INPUT_SRESET:
#if defined(PPC_DEBUG_IRQ)
            if (loglevel & CPU_LOG_INT) {
                fprintf(logfile, "%s: set the RESET IRQ state to %d\n",
                        __func__, level);
            }
#endif
            ppc_set_irq(env, PPC_INTERRUPT_RESET, level);
            break;
        case PPC970_INPUT_TBEN:
#if defined(PPC_DEBUG_IRQ)
            if (loglevel & CPU_LOG_INT) {
                fprintf(logfile, "%s: set the TBEN state to %d\n", __func__,
                        level);
            }
#endif
            /* XXX: TODO */
            break;
        default:
            /* Unknown pin - do nothing */
#if defined(PPC_DEBUG_IRQ)
            if (loglevel & CPU_LOG_INT) {
                fprintf(logfile, "%s: unknown IRQ pin %d\n", __func__, pin);
            }
#endif
            return;
        }
        if (level)
            env->irq_input_state |= 1 << pin;
        else
            env->irq_input_state &= ~(1 << pin);
    }
}

void ppc970_irq_init (CPUState *env)
{
    env->irq_inputs = (void **)qemu_allocate_irqs(&ppc970_set_irq, env, 7);
}

/* PowerPC 405 internal IRQ controller */
static void ppc405_set_irq (void *opaque, int pin, int level)
{
    CPUState *env = opaque;
    int cur_level;

#if defined(PPC_DEBUG_IRQ)
    if (loglevel & CPU_LOG_INT) {
        fprintf(logfile, "%s: env %p pin %d level %d\n", __func__,
                env, pin, level);
    }
#endif
    cur_level = (env->irq_input_state >> pin) & 1;
    /* Don't generate spurious events */
    if ((cur_level == 1 && level == 0) || (cur_level == 0 && level != 0)) {
        switch (pin) {
        case PPC405_INPUT_RESET_SYS:
            if (level) {
#if defined(PPC_DEBUG_IRQ)
                if (loglevel & CPU_LOG_INT) {
                    fprintf(logfile, "%s: reset the PowerPC system\n",
                            __func__);
                }
#endif
                ppc40x_system_reset(env);
            }
            break;
        case PPC405_INPUT_RESET_CHIP:
            if (level) {
#if defined(PPC_DEBUG_IRQ)
                if (loglevel & CPU_LOG_INT) {
                    fprintf(logfile, "%s: reset the PowerPC chip\n", __func__);
                }
#endif
                ppc40x_chip_reset(env);
            }
            break;
            /* No break here */
        case PPC405_INPUT_RESET_CORE:
            /* XXX: TODO: update DBSR[MRR] */
            if (level) {
#if defined(PPC_DEBUG_IRQ)
                if (loglevel & CPU_LOG_INT) {
                    fprintf(logfile, "%s: reset the PowerPC core\n", __func__);
                }
#endif
                ppc40x_core_reset(env);
            }
            break;
        case PPC405_INPUT_CINT:
            /* Level sensitive - active high */
#if defined(PPC_DEBUG_IRQ)
            if (loglevel & CPU_LOG_INT) {
                fprintf(logfile, "%s: set the critical IRQ state to %d\n",
                        __func__, level);
            }
#endif
            /* XXX: TOFIX */
            ppc_set_irq(env, PPC_INTERRUPT_RESET, level);
            break;
        case PPC405_INPUT_INT:
            /* Level sensitive - active high */
#if defined(PPC_DEBUG_IRQ)
            if (loglevel & CPU_LOG_INT) {
                fprintf(logfile, "%s: set the external IRQ state to %d\n",
                        __func__, level);
            }
#endif
            ppc_set_irq(env, PPC_INTERRUPT_EXT, level);
            break;
        case PPC405_INPUT_HALT:
            /* Level sensitive - active low */
            if (level) {
#if defined(PPC_DEBUG_IRQ)
                if (loglevel & CPU_LOG_INT) {
                    fprintf(logfile, "%s: stop the CPU\n", __func__);
                }
#endif
                env->halted = 1;
            } else {
#if defined(PPC_DEBUG_IRQ)
                if (loglevel & CPU_LOG_INT) {
                    fprintf(logfile, "%s: restart the CPU\n", __func__);
                }
#endif
                env->halted = 0;
            }
            break;
        case PPC405_INPUT_DEBUG:
            /* Level sensitive - active high */
#if defined(PPC_DEBUG_IRQ)
            if (loglevel & CPU_LOG_INT) {
                fprintf(logfile, "%s: set the external IRQ state to %d\n",
                        __func__, level);
            }
#endif
            ppc_set_irq(env, EXCP_40x_DEBUG, level);
            break;
        default:
            /* Unknown pin - do nothing */
#if defined(PPC_DEBUG_IRQ)
            if (loglevel & CPU_LOG_INT) {
                fprintf(logfile, "%s: unknown IRQ pin %d\n", __func__, pin);
            }
#endif
            return;
        }
        if (level)
            env->irq_input_state |= 1 << pin;
        else
            env->irq_input_state &= ~(1 << pin);
    }
}

void ppc405_irq_init (CPUState *env)
{
    env->irq_inputs = (void **)qemu_allocate_irqs(&ppc405_set_irq, env, 7);
}

/*****************************************************************************/
/* PowerPC time base and decrementer emulation */
struct ppc_tb_t {
    /* Time base management */
    int64_t  tb_offset;    /* Compensation               */
    uint32_t tb_freq;      /* TB frequency               */
    /* Decrementer management */
    uint64_t decr_next;    /* Tick for next decr interrupt  */
    struct QEMUTimer *decr_timer;
    void *opaque;
};

static inline uint64_t cpu_ppc_get_tb (ppc_tb_t *tb_env)
{
    /* TB time in tb periods */
    return muldiv64(qemu_get_clock(vm_clock) + tb_env->tb_offset,
                    tb_env->tb_freq, ticks_per_sec);
}

uint32_t cpu_ppc_load_tbl (CPUState *env)
{
    ppc_tb_t *tb_env = env->tb_env;
    uint64_t tb;

    tb = cpu_ppc_get_tb(tb_env);
#ifdef PPC_DEBUG_TB
    {
        static int last_time;
        int now;
        now = time(NULL);
        if (last_time != now) {
            last_time = now;
            if (loglevel != 0) {
                fprintf(logfile, "%s: tb=0x%016lx %d %08lx\n",
                        __func__, tb, now, tb_env->tb_offset);
            }
        }
    }
#endif

    return tb & 0xFFFFFFFF;
}

uint32_t cpu_ppc_load_tbu (CPUState *env)
{
    ppc_tb_t *tb_env = env->tb_env;
    uint64_t tb;

    tb = cpu_ppc_get_tb(tb_env);
#if defined(PPC_DEBUG_TB)
    if (loglevel != 0) {
        fprintf(logfile, "%s: tb=0x%016lx\n", __func__, tb);
    }
#endif

    return tb >> 32;
}

static void cpu_ppc_store_tb (ppc_tb_t *tb_env, uint64_t value)
{
    tb_env->tb_offset = muldiv64(value, ticks_per_sec, tb_env->tb_freq)
        - qemu_get_clock(vm_clock);
#ifdef PPC_DEBUG_TB
    if (loglevel != 0) {
        fprintf(logfile, "%s: tb=0x%016lx offset=%08lx\n", __func__, value,
                tb_env->tb_offset);
    }
#endif
}

void cpu_ppc_store_tbu (CPUState *env, uint32_t value)
{
    ppc_tb_t *tb_env = env->tb_env;

    cpu_ppc_store_tb(tb_env,
                     ((uint64_t)value << 32) | cpu_ppc_load_tbl(env));
}

void cpu_ppc_store_tbl (CPUState *env, uint32_t value)
{
    ppc_tb_t *tb_env = env->tb_env;

    cpu_ppc_store_tb(tb_env,
                     ((uint64_t)cpu_ppc_load_tbu(env) << 32) | value);
}

uint32_t cpu_ppc_load_decr (CPUState *env)
{
    ppc_tb_t *tb_env = env->tb_env;
    uint32_t decr;
    int64_t diff;

    diff = tb_env->decr_next - qemu_get_clock(vm_clock);
    if (diff >= 0)
        decr = muldiv64(diff, tb_env->tb_freq, ticks_per_sec);
    else
        decr = -muldiv64(-diff, tb_env->tb_freq, ticks_per_sec);
#if defined(PPC_DEBUG_TB)
    if (loglevel != 0) {
        fprintf(logfile, "%s: 0x%08x\n", __func__, decr);
    }
#endif

    return decr;
}

/* When decrementer expires,
 * all we need to do is generate or queue a CPU exception
 */
static inline void cpu_ppc_decr_excp (CPUState *env)
{
    /* Raise it */
#ifdef PPC_DEBUG_TB
    if (loglevel != 0) {
        fprintf(logfile, "raise decrementer exception\n");
    }
#endif
    ppc_set_irq(env, PPC_INTERRUPT_DECR, 1);
}

static void _cpu_ppc_store_decr (CPUState *env, uint32_t decr,
                                 uint32_t value, int is_excp)
{
    ppc_tb_t *tb_env = env->tb_env;
    uint64_t now, next;

#ifdef PPC_DEBUG_TB
    if (loglevel != 0) {
        fprintf(logfile, "%s: 0x%08x => 0x%08x\n", __func__, decr, value);
    }
#endif
    now = qemu_get_clock(vm_clock);
    next = now + muldiv64(value, ticks_per_sec, tb_env->tb_freq);
    if (is_excp)
        next += tb_env->decr_next - now;
    if (next == now)
        next++;
    tb_env->decr_next = next;
    /* Adjust timer */
    qemu_mod_timer(tb_env->decr_timer, next);
    /* If we set a negative value and the decrementer was positive,
     * raise an exception.
     */
    if ((value & 0x80000000) && !(decr & 0x80000000))
        cpu_ppc_decr_excp(env);
}

void cpu_ppc_store_decr (CPUState *env, uint32_t value)
{
    _cpu_ppc_store_decr(env, cpu_ppc_load_decr(env), value, 0);
}

static void cpu_ppc_decr_cb (void *opaque)
{
    _cpu_ppc_store_decr(opaque, 0x00000000, 0xFFFFFFFF, 1);
}

static void cpu_ppc_set_tb_clk (void *opaque, uint32_t freq)
{
    CPUState *env = opaque;
    ppc_tb_t *tb_env = env->tb_env;

    tb_env->tb_freq = freq;
    /* There is a bug in Linux 2.4 kernels:
     * if a decrementer exception is pending when it enables msr_ee at startup,
     * it's not ready to handle it...
     */
    _cpu_ppc_store_decr(env, 0xFFFFFFFF, 0xFFFFFFFF, 0);
}

/* Set up (once) timebase frequency (in Hz) */
clk_setup_cb cpu_ppc_tb_init (CPUState *env, uint32_t freq)
{
    ppc_tb_t *tb_env;

    tb_env = qemu_mallocz(sizeof(ppc_tb_t));
    if (tb_env == NULL)
        return NULL;
    env->tb_env = tb_env;
    /* Create new timer */
    tb_env->decr_timer = qemu_new_timer(vm_clock, &cpu_ppc_decr_cb, env);
    cpu_ppc_set_tb_clk(env, freq);

    return &cpu_ppc_set_tb_clk;
}

/* Specific helpers for POWER & PowerPC 601 RTC */
clk_setup_cb cpu_ppc601_rtc_init (CPUState *env)
{
    return cpu_ppc_tb_init(env, 7812500);
}

void cpu_ppc601_store_rtcu (CPUState *env, uint32_t value)
__attribute__ (( alias ("cpu_ppc_store_tbu") ));

uint32_t cpu_ppc601_load_rtcu (CPUState *env)
__attribute__ (( alias ("cpu_ppc_load_tbu") ));

void cpu_ppc601_store_rtcl (CPUState *env, uint32_t value)
{
    cpu_ppc_store_tbl(env, value & 0x3FFFFF80);
}

uint32_t cpu_ppc601_load_rtcl (CPUState *env)
{
    return cpu_ppc_load_tbl(env) & 0x3FFFFF80;
}

/*****************************************************************************/
/* Embedded PowerPC timers */

/* PIT, FIT & WDT */
typedef struct ppcemb_timer_t ppcemb_timer_t;
struct ppcemb_timer_t {
    uint64_t pit_reload;  /* PIT auto-reload value        */
    uint64_t fit_next;    /* Tick for next FIT interrupt  */
    struct QEMUTimer *fit_timer;
    uint64_t wdt_next;    /* Tick for next WDT interrupt  */
    struct QEMUTimer *wdt_timer;
};
   
/* Fixed interval timer */
static void cpu_4xx_fit_cb (void *opaque)
{
    CPUState *env;
    ppc_tb_t *tb_env;
    ppcemb_timer_t *ppcemb_timer;
    uint64_t now, next;

    env = opaque;
    tb_env = env->tb_env;
    ppcemb_timer = tb_env->opaque;
    now = qemu_get_clock(vm_clock);
    switch ((env->spr[SPR_40x_TCR] >> 24) & 0x3) {
    case 0:
        next = 1 << 9;
        break;
    case 1:
        next = 1 << 13;
        break;
    case 2:
        next = 1 << 17;
        break;
    case 3:
        next = 1 << 21;
        break;
    default:
        /* Cannot occur, but makes gcc happy */
        return;
    }
    next = now + muldiv64(next, ticks_per_sec, tb_env->tb_freq);
    if (next == now)
        next++;
    qemu_mod_timer(ppcemb_timer->fit_timer, next);
    env->spr[SPR_40x_TSR] |= 1 << 26;
    if ((env->spr[SPR_40x_TCR] >> 23) & 0x1)
        ppc_set_irq(env, PPC_INTERRUPT_FIT, 1);
#ifdef PPC_DEBUG_TB
    if (loglevel != 0) {
        fprintf(logfile, "%s: ir %d TCR " ADDRX " TSR " ADDRX "\n", __func__,
                (int)((env->spr[SPR_40x_TCR] >> 23) & 0x1),
                env->spr[SPR_40x_TCR], env->spr[SPR_40x_TSR]);
    }
#endif
}

/* Programmable interval timer */
static void start_stop_pit (CPUState *env, ppc_tb_t *tb_env, int is_excp)
{
    ppcemb_timer_t *ppcemb_timer;
    uint64_t now, next;

    ppcemb_timer = tb_env->opaque;
    if (ppcemb_timer->pit_reload <= 1 ||
        !((env->spr[SPR_40x_TCR] >> 26) & 0x1) ||
        (is_excp && !((env->spr[SPR_40x_TCR] >> 22) & 0x1))) {
        /* Stop PIT */
#ifdef PPC_DEBUG_TB
        if (loglevel != 0) {
            fprintf(logfile, "%s: stop PIT\n", __func__);
        }
#endif
        qemu_del_timer(tb_env->decr_timer);
    } else {
#ifdef PPC_DEBUG_TB
        if (loglevel != 0) {
            fprintf(logfile, "%s: start PIT 0x" REGX "\n",
                    __func__, ppcemb_timer->pit_reload);
        }
#endif
        now = qemu_get_clock(vm_clock);
        next = now + muldiv64(ppcemb_timer->pit_reload,
                              ticks_per_sec, tb_env->tb_freq);
        if (is_excp)
            next += tb_env->decr_next - now;
        if (next == now)
            next++;
        qemu_mod_timer(tb_env->decr_timer, next);
        tb_env->decr_next = next;
    }
}

static void cpu_4xx_pit_cb (void *opaque)
{
    CPUState *env;
    ppc_tb_t *tb_env;
    ppcemb_timer_t *ppcemb_timer;

    env = opaque;
    tb_env = env->tb_env;
    ppcemb_timer = tb_env->opaque;
    env->spr[SPR_40x_TSR] |= 1 << 27;
    if ((env->spr[SPR_40x_TCR] >> 26) & 0x1)
        ppc_set_irq(env, PPC_INTERRUPT_PIT, 1);
    start_stop_pit(env, tb_env, 1);
#ifdef PPC_DEBUG_TB
    if (loglevel != 0) {
        fprintf(logfile, "%s: ar %d ir %d TCR " ADDRX " TSR " ADDRX " "
                "%016" PRIx64 "\n", __func__,
                (int)((env->spr[SPR_40x_TCR] >> 22) & 0x1),
                (int)((env->spr[SPR_40x_TCR] >> 26) & 0x1),
                env->spr[SPR_40x_TCR], env->spr[SPR_40x_TSR],
                ppcemb_timer->pit_reload);
    }
#endif
}

/* Watchdog timer */
static void cpu_4xx_wdt_cb (void *opaque)
{
    CPUState *env;
    ppc_tb_t *tb_env;
    ppcemb_timer_t *ppcemb_timer;
    uint64_t now, next;

    env = opaque;
    tb_env = env->tb_env;
    ppcemb_timer = tb_env->opaque;
    now = qemu_get_clock(vm_clock);
    switch ((env->spr[SPR_40x_TCR] >> 30) & 0x3) {
    case 0:
        next = 1 << 17;
        break;
    case 1:
        next = 1 << 21;
        break;
    case 2:
        next = 1 << 25;
        break;
    case 3:
        next = 1 << 29;
        break;
    default:
        /* Cannot occur, but makes gcc happy */
        return;
    }
    next = now + muldiv64(next, ticks_per_sec, tb_env->tb_freq);
    if (next == now)
        next++;
#ifdef PPC_DEBUG_TB
    if (loglevel != 0) {
        fprintf(logfile, "%s: TCR " ADDRX " TSR " ADDRX "\n", __func__,
                env->spr[SPR_40x_TCR], env->spr[SPR_40x_TSR]);
    }
#endif
    switch ((env->spr[SPR_40x_TSR] >> 30) & 0x3) {
    case 0x0:
    case 0x1:
        qemu_mod_timer(ppcemb_timer->wdt_timer, next);
        ppcemb_timer->wdt_next = next;
        env->spr[SPR_40x_TSR] |= 1 << 31;
        break;
    case 0x2:
        qemu_mod_timer(ppcemb_timer->wdt_timer, next);
        ppcemb_timer->wdt_next = next;
        env->spr[SPR_40x_TSR] |= 1 << 30;
        if ((env->spr[SPR_40x_TCR] >> 27) & 0x1)
            ppc_set_irq(env, PPC_INTERRUPT_WDT, 1);
        break;
    case 0x3:
        env->spr[SPR_40x_TSR] &= ~0x30000000;
        env->spr[SPR_40x_TSR] |= env->spr[SPR_40x_TCR] & 0x30000000;
        switch ((env->spr[SPR_40x_TCR] >> 28) & 0x3) {
        case 0x0:
            /* No reset */
            break;
        case 0x1: /* Core reset */
            ppc40x_core_reset(env);
            break;
        case 0x2: /* Chip reset */
            ppc40x_chip_reset(env);
            break;
        case 0x3: /* System reset */
            ppc40x_system_reset(env);
            break;
        }
    }
}

void store_40x_pit (CPUState *env, target_ulong val)
{
    ppc_tb_t *tb_env;
    ppcemb_timer_t *ppcemb_timer;

    tb_env = env->tb_env;
    ppcemb_timer = tb_env->opaque;
#ifdef PPC_DEBUG_TB
    if (loglevel != 0) {
        fprintf(logfile, "%s %p %p\n", __func__, tb_env, ppcemb_timer);
    }
#endif
    ppcemb_timer->pit_reload = val;
    start_stop_pit(env, tb_env, 0);
}

target_ulong load_40x_pit (CPUState *env)
{
    return cpu_ppc_load_decr(env);
}

void store_booke_tsr (CPUState *env, target_ulong val)
{
#ifdef PPC_DEBUG_TB
    if (loglevel != 0) {
        fprintf(logfile, "%s: val=" ADDRX "\n", __func__, val);
    }
#endif
    env->spr[SPR_40x_TSR] &= ~(val & 0xFC000000);
    if (val & 0x80000000)
        ppc_set_irq(env, PPC_INTERRUPT_PIT, 0);
}

void store_booke_tcr (CPUState *env, target_ulong val)
{
    ppc_tb_t *tb_env;

    tb_env = env->tb_env;
#ifdef PPC_DEBUG_TB
    if (loglevel != 0) {
        fprintf(logfile, "%s: val=" ADDRX "\n", __func__, val);
    }
#endif
    env->spr[SPR_40x_TCR] = val & 0xFFC00000;
    start_stop_pit(env, tb_env, 1);
    cpu_4xx_wdt_cb(env);
}

static void ppc_emb_set_tb_clk (void *opaque, uint32_t freq)
{
    CPUState *env = opaque;
    ppc_tb_t *tb_env = env->tb_env;

#ifdef PPC_DEBUG_TB
    if (loglevel != 0) {
        fprintf(logfile, "%s set new frequency to %u\n", __func__, freq);
    }
#endif
    tb_env->tb_freq = freq;
    /* XXX: we should also update all timers */
}

clk_setup_cb ppc_emb_timers_init (CPUState *env, uint32_t freq)
{
    ppc_tb_t *tb_env;
    ppcemb_timer_t *ppcemb_timer;

    tb_env = qemu_mallocz(sizeof(ppc_tb_t));
    if (tb_env == NULL) {
        return NULL;
    }
    env->tb_env = tb_env;
    ppcemb_timer = qemu_mallocz(sizeof(ppcemb_timer_t));
    tb_env->tb_freq = freq;
    tb_env->opaque = ppcemb_timer;
#ifdef PPC_DEBUG_TB
    if (loglevel != 0) {
        fprintf(logfile, "%s %p %p %p\n", __func__, tb_env, ppcemb_timer,
                &ppc_emb_set_tb_clk);
    }
#endif
    if (ppcemb_timer != NULL) {
        /* We use decr timer for PIT */
        tb_env->decr_timer = qemu_new_timer(vm_clock, &cpu_4xx_pit_cb, env);
        ppcemb_timer->fit_timer =
            qemu_new_timer(vm_clock, &cpu_4xx_fit_cb, env);
        ppcemb_timer->wdt_timer =
            qemu_new_timer(vm_clock, &cpu_4xx_wdt_cb, env);
    }

    return &ppc_emb_set_tb_clk;
}

/*****************************************************************************/
/* Embedded PowerPC Device Control Registers */
typedef struct ppc_dcrn_t ppc_dcrn_t;
struct ppc_dcrn_t {
    dcr_read_cb dcr_read;
    dcr_write_cb dcr_write;
    void *opaque;
};

#define DCRN_NB 1024
struct ppc_dcr_t {
    ppc_dcrn_t dcrn[DCRN_NB];
    int (*read_error)(int dcrn);
    int (*write_error)(int dcrn);
};

int ppc_dcr_read (ppc_dcr_t *dcr_env, int dcrn, target_ulong *valp)
{
    ppc_dcrn_t *dcr;

    if (dcrn < 0 || dcrn >= DCRN_NB)
        goto error;
    dcr = &dcr_env->dcrn[dcrn];
    if (dcr->dcr_read == NULL)
        goto error;
    *valp = (*dcr->dcr_read)(dcr->opaque, dcrn);

    return 0;

 error:
    if (dcr_env->read_error != NULL)
        return (*dcr_env->read_error)(dcrn);

    return -1;
}

int ppc_dcr_write (ppc_dcr_t *dcr_env, int dcrn, target_ulong val)
{
    ppc_dcrn_t *dcr;

    if (dcrn < 0 || dcrn >= DCRN_NB)
        goto error;
    dcr = &dcr_env->dcrn[dcrn];
    if (dcr->dcr_write == NULL)
        goto error;
    (*dcr->dcr_write)(dcr->opaque, dcrn, val);

    return 0;

 error:
    if (dcr_env->write_error != NULL)
        return (*dcr_env->write_error)(dcrn);

    return -1;
}

int ppc_dcr_register (CPUState *env, int dcrn, void *opaque,
                      dcr_read_cb dcr_read, dcr_write_cb dcr_write)
{
    ppc_dcr_t *dcr_env;
    ppc_dcrn_t *dcr;

    dcr_env = env->dcr_env;
    if (dcr_env == NULL)
        return -1;
    if (dcrn < 0 || dcrn >= DCRN_NB)
        return -1;
    dcr = &dcr_env->dcrn[dcrn];
    if (dcr->opaque != NULL ||
        dcr->dcr_read != NULL ||
        dcr->dcr_write != NULL)
        return -1;
    dcr->opaque = opaque;
    dcr->dcr_read = dcr_read;
    dcr->dcr_write = dcr_write;

    return 0;
}

int ppc_dcr_init (CPUState *env, int (*read_error)(int dcrn),
                  int (*write_error)(int dcrn))
{
    ppc_dcr_t *dcr_env;

    dcr_env = qemu_mallocz(sizeof(ppc_dcr_t));
    if (dcr_env == NULL)
        return -1;
    dcr_env->read_error = read_error;
    dcr_env->write_error = write_error;
    env->dcr_env = dcr_env;

    return 0;
}


#if 0
/*****************************************************************************/
/* Handle system reset (for now, just stop emulation) */
void cpu_ppc_reset (CPUState *env)
{
    printf("Reset asked... Stop emulation\n");
    abort();
}
#endif

/*****************************************************************************/
/* Debug port */
void PPC_debug_write (void *opaque, uint32_t addr, uint32_t val)
{
    addr &= 0xF;
    switch (addr) {
    case 0:
        printf("%c", val);
        break;
    case 1:
        printf("\n");
        fflush(stdout);
        break;
    case 2:
        printf("Set loglevel to %04x\n", val);
        cpu_set_log(val | 0x100);
        break;
    }
}

/*****************************************************************************/
/* NVRAM helpers */
void NVRAM_set_byte (m48t59_t *nvram, uint32_t addr, uint8_t value)
{
    m48t59_write(nvram, addr, value);
}

uint8_t NVRAM_get_byte (m48t59_t *nvram, uint32_t addr)
{
    return m48t59_read(nvram, addr);
}

void NVRAM_set_word (m48t59_t *nvram, uint32_t addr, uint16_t value)
{
    m48t59_write(nvram, addr, value >> 8);
    m48t59_write(nvram, addr + 1, value & 0xFF);
}

uint16_t NVRAM_get_word (m48t59_t *nvram, uint32_t addr)
{
    uint16_t tmp;

    tmp = m48t59_read(nvram, addr) << 8;
    tmp |= m48t59_read(nvram, addr + 1);
    return tmp;
}

void NVRAM_set_lword (m48t59_t *nvram, uint32_t addr, uint32_t value)
{
    m48t59_write(nvram, addr, value >> 24);
    m48t59_write(nvram, addr + 1, (value >> 16) & 0xFF);
    m48t59_write(nvram, addr + 2, (value >> 8) & 0xFF);
    m48t59_write(nvram, addr + 3, value & 0xFF);
}

uint32_t NVRAM_get_lword (m48t59_t *nvram, uint32_t addr)
{
    uint32_t tmp;

    tmp = m48t59_read(nvram, addr) << 24;
    tmp |= m48t59_read(nvram, addr + 1) << 16;
    tmp |= m48t59_read(nvram, addr + 2) << 8;
    tmp |= m48t59_read(nvram, addr + 3);

    return tmp;
}

void NVRAM_set_string (m48t59_t *nvram, uint32_t addr,
                       const unsigned char *str, uint32_t max)
{
    int i;

    for (i = 0; i < max && str[i] != '\0'; i++) {
        m48t59_write(nvram, addr + i, str[i]);
    }
    m48t59_write(nvram, addr + max - 1, '\0');
}

int NVRAM_get_string (m48t59_t *nvram, uint8_t *dst, uint16_t addr, int max)
{
    int i;

    memset(dst, 0, max);
    for (i = 0; i < max; i++) {
        dst[i] = NVRAM_get_byte(nvram, addr + i);
        if (dst[i] == '\0')
            break;
    }

    return i;
}

static uint16_t NVRAM_crc_update (uint16_t prev, uint16_t value)
{
    uint16_t tmp;
    uint16_t pd, pd1, pd2;

    tmp = prev >> 8;
    pd = prev ^ value;
    pd1 = pd & 0x000F;
    pd2 = ((pd >> 4) & 0x000F) ^ pd1;
    tmp ^= (pd1 << 3) | (pd1 << 8);
    tmp ^= pd2 | (pd2 << 7) | (pd2 << 12);

    return tmp;
}

uint16_t NVRAM_compute_crc (m48t59_t *nvram, uint32_t start, uint32_t count)
{
    uint32_t i;
    uint16_t crc = 0xFFFF;
    int odd;

    odd = count & 1;
    count &= ~1;
    for (i = 0; i != count; i++) {
        crc = NVRAM_crc_update(crc, NVRAM_get_word(nvram, start + i));
    }
    if (odd) {
        crc = NVRAM_crc_update(crc, NVRAM_get_byte(nvram, start + i) << 8);
    }

    return crc;
}

#define CMDLINE_ADDR 0x017ff000

int PPC_NVRAM_set_params (m48t59_t *nvram, uint16_t NVRAM_size,
                          const unsigned char *arch,
                          uint32_t RAM_size, int boot_device,
                          uint32_t kernel_image, uint32_t kernel_size,
                          const char *cmdline,
                          uint32_t initrd_image, uint32_t initrd_size,
                          uint32_t NVRAM_image,
                          int width, int height, int depth)
{
    uint16_t crc;

    /* Set parameters for Open Hack'Ware BIOS */
    NVRAM_set_string(nvram, 0x00, "QEMU_BIOS", 16);
    NVRAM_set_lword(nvram,  0x10, 0x00000002); /* structure v2 */
    NVRAM_set_word(nvram,   0x14, NVRAM_size);
    NVRAM_set_string(nvram, 0x20, arch, 16);
    NVRAM_set_lword(nvram,  0x30, RAM_size);
    NVRAM_set_byte(nvram,   0x34, boot_device);
    NVRAM_set_lword(nvram,  0x38, kernel_image);
    NVRAM_set_lword(nvram,  0x3C, kernel_size);
    if (cmdline) {
        /* XXX: put the cmdline in NVRAM too ? */
        strcpy(phys_ram_base + CMDLINE_ADDR, cmdline);
        NVRAM_set_lword(nvram,  0x40, CMDLINE_ADDR);
        NVRAM_set_lword(nvram,  0x44, strlen(cmdline));
    } else {
        NVRAM_set_lword(nvram,  0x40, 0);
        NVRAM_set_lword(nvram,  0x44, 0);
    }
    NVRAM_set_lword(nvram,  0x48, initrd_image);
    NVRAM_set_lword(nvram,  0x4C, initrd_size);
    NVRAM_set_lword(nvram,  0x50, NVRAM_image);

    NVRAM_set_word(nvram,   0x54, width);
    NVRAM_set_word(nvram,   0x56, height);
    NVRAM_set_word(nvram,   0x58, depth);
    crc = NVRAM_compute_crc(nvram, 0x00, 0xF8);
    NVRAM_set_word(nvram,  0xFC, crc);

    return 0;
}
