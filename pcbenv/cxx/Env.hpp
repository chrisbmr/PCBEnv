
#ifndef GYM_PCB_ENV_H
#define GYM_PCB_ENV_H

class Env
{
public:
    Env();
    virtual ~Env();

    virtual PyObject *reset() = 0;

    virtual PyObject *step(PyObject *action) = 0;

    virtual void render(PyObject *command) = 0;

    virtual void seed(PyObject *) = 0;

    virtual void close() { }

    virtual PyObject *set_task(PyObject *) = 0;

    virtual PyObject *run_agent(PyObject *) = 0;

    virtual PyObject *set_agent(PyObject *) = 0;

    virtual PyObject *get_state(PyObject *spec) = 0;

    virtual PyObject *__str__() const = 0;

    virtual Env *__enter__() { return this; }

    virtual bool __exit__() { close(); return false; }
};

Env *create_env(PyObject *spec);

#endif // GYM_PCB_ENV_H
