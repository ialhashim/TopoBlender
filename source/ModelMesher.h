#pragma once

class Model;

class ModelMesher
{
public:
    ModelMesher(Model *);

    void generateOffsetSurface(double offset);
    void generateRegularSurface(double offset);

private:
    Model * m;
};
