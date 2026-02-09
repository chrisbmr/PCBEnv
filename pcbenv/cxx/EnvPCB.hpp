
#ifndef GYM_PCB_ENVPCB_H
#define GYM_PCB_ENVPCB_H

#include "Py.hpp"
#include "Env.hpp"
#include <memory>
#include <set>
#include <vector>

class Agent;
class PCBoard;
class Component;
class Pin;
class Net;
class Connection;

class IUIApplication;

class EnvPCB : public Env
{
public:
    EnvPCB();
    ~EnvPCB();

    PyObject *reset() override;

    PyObject *step(PyObject *action) override;

    void render(PyObject *mode) override;

    void seed(PyObject *) override;

    void close() override;

    PyObject *set_task(PyObject *) override;

    PyObject *run_agent(PyObject *) override;

    PyObject *set_agent(PyObject *) override;

    PyObject *get_state(PyObject *spec) override;

    PyObject *__str__() const override;

private:
    std::shared_ptr<PCBoard> mBoard;
    std::shared_ptr<Agent> mAgent;
    std::unique_ptr<IUIApplication> mUI;
};

#endif // GYM_PCB_ENVPCB_H
