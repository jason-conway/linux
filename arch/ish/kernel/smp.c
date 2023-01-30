#include <linux/cpu.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/sched/task_stack.h>
#include <linux/smp.h>
#include <asm/smp.h>
#include <user/user.h>
#include <user/irq.h>
#include "irq_user.h"
#include "time_user.h"
#include "threads_user.h"

void __init smp_prepare_boot_cpu(void)
{
}

static irqreturn_t ipi_irq(int irq, void *dev);

void __init smp_prepare_cpus(unsigned int max_cpus)
{
	unsigned i;
	int err;
	int num_threads = host_get_nproc();

	if (max_cpus > num_threads)
		max_cpus = num_threads;
	for (i = 0; i < max_cpus; i++)
		set_cpu_present(i, true);

	irq_set_handler(IPI_RESCHEDULE, handle_percpu_irq);
	irq_set_handler(IPI_CALL_FUNC, handle_percpu_irq);
	err = request_irq(IPI_RESCHEDULE, ipi_irq, IRQF_PERCPU, "ipi-resched", NULL);
	if (err < 0)
		panic("failed to reqest IPI_RESCHEDULE irq");
	err = request_irq(IPI_CALL_FUNC, ipi_irq, IRQF_PERCPU, "ipi-call-func", NULL);
	if (err < 0)
		panic("failed to reqest IPI_CALL_FUNC irq");
}

void start_secondary(void *arg)
{
	unsigned cpu;

	user_setup_thread();
	__set_current(arg);
	cpu = smp_processor_id();
	local_timer_init();

	/* As far as I can tell, you should never write a start_secondary
	 * without the following. This is never explained. */
	mmgrab(&init_mm);
	current->active_mm = &init_mm;
	preempt_disable(); /* have no idea if this is needed */
	notify_cpu_starting(cpu);
	set_cpu_online(cpu, true);
	local_irq_enable();
	cpu_startup_entry(CPUHP_AP_ONLINE_IDLE);

	BUG();
}

int __cpu_up(unsigned int cpu, struct task_struct *idle)
{
	int err;
	task_thread_info(idle)->cpu = cpu;
	err = start_cpu_thread(cpu, start_secondary, idle, task_stack_page(idle), THREAD_SIZE);
	if (err != 0) {
		printk("failed to start thread for cpu %d: %pe", cpu, ERR_PTR(err));
		return err;
	}
	return 0;
}

void __init smp_cpus_done(unsigned int max_cpus)
{
}

static irqreturn_t ipi_irq(int irq, void *dev)
{
	switch (irq) {
	case IPI_RESCHEDULE:
		scheduler_ipi();
		break;
	case IPI_CALL_FUNC:
		generic_smp_call_function_interrupt();
		break;
	default:
		BUG();
	}
	return IRQ_HANDLED;
}

void smp_send_stop(void)
{
	printk("TODO: actually stop the threads\n");
}

void smp_send_reschedule(int cpu)
{
	trigger_irq_on_cpu(IPI_RESCHEDULE, cpu);
}

void arch_send_call_function_ipi_mask(const struct cpumask *mask)
{
	int cpu;
	for_each_cpu(cpu, mask) {
		arch_send_call_function_single_ipi(cpu);
	}
}

void arch_send_call_function_single_ipi(int cpu)
{
	trigger_irq_on_cpu(IPI_CALL_FUNC, cpu);
}
