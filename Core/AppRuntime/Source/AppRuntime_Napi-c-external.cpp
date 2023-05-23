#include "AppRuntime.h"
#include "WorkQueue.h"
#include <exception>
#include <sstream>
#include <functional>
#include <napi/js_native_api.h>
#include <napi/js_native_ext_api.h>

namespace Babylon
{
    void ScheduleTaskCallback(void *task_runner_data, void *task_data, v8_task_run_cb task_run_cb, v8_task_release_cb task_release_cb)
    {
        WorkQueue* worker = reinterpret_cast<WorkQueue*>(task_runner_data);

        worker->Append([task = std::move(task_run_cb), task_data, task_release_cb](Napi::Env)
        { 
            task(task_data);
            task_release_cb(task_data);
        });
    }

    void __cdecl v8TaskRunnerReleaseCb(void *)
    {

    }

    void AppRuntime::RunEnvironmentTier(const char*)
    {
        napi_env _env{};
        napi_ext_env_settings settings{};
        settings.this_size = sizeof(settings);
        settings.flags.enable_inspector = true;
        settings.flags.wait_for_debugger = false;
        settings.foreground_task_runner = v8_create_task_runner(this->m_workQueue.get(), &ScheduleTaskCallback, &v8TaskRunnerReleaseCb);

        napi_status result = napi_ext_create_env(&settings, &_env);
        assert(result == napi_status::napi_ok);

        Napi::Env env = Napi::Env(_env);

        napi_ext_env_scope scope;
        result = napi_ext_open_env_scope(env, &scope);
        assert(result == napi_status::napi_ok);

        Run(env);

        result = napi_ext_close_env_scope(env, scope);
        assert(result == napi_status::napi_ok);
    }
}