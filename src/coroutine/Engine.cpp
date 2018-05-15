#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

namespace Afina {
namespace Coroutine {


void Engine::Store(context &ctx)
{
    char cur_stack_pos;

    ctx.Low = &cur_stack_pos;
    ctx.High = StackBottom;

    size_t stack_size = ctx.High - ctx.Low;

    delete std::get<0>(ctx.Stack); // free saved stack array

    char* stack_saved = new char[stack_size];
    memcpy(stack_saved, ctx.Low, stack_size);// copy current stack

    ctx.Stack = std::make_tuple(stack_saved, stack_size);
}



void Engine::Restore(context &ctx)
{
    char stack_pos;
    if (&stack_pos >= ctx.Low)
    {
        Restore(ctx);
    }

    char* saved_stack = std::get<0>(ctx.Stack);
    size_t  stack_size = std::get<1>(ctx.Stack);
    memcpy(ctx.Low, saved_stack, stack_size); // copy saved stack to current context


    longjmp(ctx.Environment, 1);
}

void Engine::yield()
{
    context *routine = alive;

    if (routine == nullptr)
    {
        return;
    }

    if (routine == cur_routine)
    {
        sched(routine->next);
    }

    sched(routine);
}

void Engine::sched(void *routine_)
{

    context *ctx = (context*) routine_;

    if (cur_routine != nullptr)
    {
        if (setjmp(cur_routine->Environment) > 0)
        {
            return;
        }

        Store(*cur_routine);
    }

    cur_routine = ctx;
    Restore(*cur_routine);
}


} // namespace Coroutine
} // namespace Afina
