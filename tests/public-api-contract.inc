<?php

/** @param ReflectionClass<object>|null $context */
function perfidious_contract_type(?ReflectionType $type, ?ReflectionClass $context = null): ?string
{
    if ($type === null) {
        return null;
    }

    if ($type instanceof ReflectionNamedType) {
        $name = $type->getName();

        if ($context !== null && $name === 'self') {
            $name = $context->getName();
        } elseif ($context !== null && $name === 'parent') {
            $parent = $context->getParentClass();
            $name = $parent === false ? 'parent' : $parent->getName();
        }

        if ($type->allowsNull() && $name !== 'mixed' && $name !== 'null') {
            return '?' . $name;
        }

        return $name;
    }

    assert($type instanceof ReflectionUnionType || $type instanceof ReflectionIntersectionType);

    $parts = array_map(
        static fn(ReflectionType $part): string => perfidious_contract_type($part, $context) ?? 'null',
        $type->getTypes(),
    );

    $separator = $type instanceof ReflectionIntersectionType ? '&' : '|';
    return implode($separator, $parts);
}

/** @return array{constant: string}|array{value: mixed}|null */
function perfidious_contract_parameter_default(ReflectionParameter $parameter): ?array
{
    if (!$parameter->isDefaultValueAvailable()) {
        return null;
    }

    if ($parameter->isDefaultValueConstant()) {
        $constantName = $parameter->getDefaultValueConstantName();
        assert(is_string($constantName));
        return ['constant' => $constantName];
    }

    return ['value' => $parameter->getDefaultValue()];
}

/** @return array<string, mixed> */
function perfidious_contract_callable(ReflectionFunctionAbstract $function): array
{
    $context = $function instanceof ReflectionMethod ? $function->getDeclaringClass() : null;
    $parameters = [];

    foreach ($function->getParameters() as $parameter) {
        $parameters[] = [
            'name' => $parameter->getName(),
            'type' => perfidious_contract_type($parameter->getType(), $context),
            'optional' => $parameter->isOptional(),
            'default' => perfidious_contract_parameter_default($parameter),
            'byReference' => $parameter->isPassedByReference(),
            'variadic' => $parameter->isVariadic(),
        ];
    }

    return [
        'returnType' => perfidious_contract_type($function->getReturnType(), $context),
        'returnsReference' => $function->returnsReference(),
        'requiredParameters' => $function->getNumberOfRequiredParameters(),
        'parameters' => $parameters,
    ];
}

function perfidious_contract_is_public_symbol(string $name, string $kind): bool
{
    if ($kind === 'function' && str_starts_with(strtolower($name), 'perfidious\\debug_')) {
        return false;
    }

    if ($kind === 'constant' && $name === 'Perfidious\\UINT64_MAX') {
        return false;
    }

    return str_starts_with($name, 'Perfidious\\');
}

/** @return array<string, ReflectionFunction> */
function perfidious_contract_functions(bool $extensionLoaded): array
{
    if ($extensionLoaded) {
        return (new ReflectionExtension('perfidious'))->getFunctions();
    }

    $functions = [];
    foreach (get_defined_functions()['user'] as $name) {
        if (str_starts_with(strtolower($name), 'perfidious\\')) {
            $reflection = new ReflectionFunction($name);
            $functions[$reflection->getName()] = $reflection;
        }
    }

    return $functions;
}

/** @return array<string, ReflectionClass<object>> */
function perfidious_contract_classes(bool $extensionLoaded): array
{
    if ($extensionLoaded) {
        return (new ReflectionExtension('perfidious'))->getClasses();
    }

    $classes = [];
    $declared = array_merge(get_declared_classes(), get_declared_interfaces());
    foreach ($declared as $name) {
        if (str_starts_with($name, 'Perfidious\\')) {
            $reflection = new ReflectionClass($name);
            $classes[$reflection->getName()] = $reflection;
        }
    }

    return $classes;
}

/**
 * @param ReflectionClass<object> $class
 * @return array<string, mixed>
 */
function perfidious_contract_class(ReflectionClass $class): array
{
    $interfaces = $class->getInterfaceNames();
    sort($interfaces);

    $methods = [];
    foreach ($class->getMethods() as $method) {
        if ($method->getDeclaringClass()->getName() !== $class->getName()) {
            continue;
        }
        if (str_starts_with(strtolower($method->getName()), 'debug')) {
            continue;
        }

        $methods[$method->getName()] = array_merge(
            [
                'visibility' => $method->isPublic() ? 'public' : ($method->isProtected() ? 'protected' : 'private'),
                'static' => $method->isStatic(),
                'final' => $method->isFinal(),
            ],
            perfidious_contract_callable($method),
        );
    }
    ksort($methods);

    $properties = [];
    foreach ($class->getProperties() as $property) {
        if ($property->getDeclaringClass()->getName() !== $class->getName()) {
            continue;
        }

        $properties[$property->getName()] = [
            'visibility' => $property->isPublic() ? 'public' : ($property->isProtected() ? 'protected' : 'private'),
            'static' => $property->isStatic(),
            'readonly' => $property->isReadOnly(),
            'type' => perfidious_contract_type($property->getType(), $class),
        ];
    }
    ksort($properties);

    $cases = [];
    if ($class->isEnum()) {
        foreach ($class->getReflectionConstants() as $case) {
            if (!$case->isEnumCase()) {
                continue;
            }

            $value = $case->getValue();
            $cases[$case->getName()] = $value instanceof BackedEnum ? $value->value : $case->getName();
        }
        ksort($cases);
    }

    $parent = $class->getParentClass();

    return [
        'kind' => $class->isEnum() ? 'enum' : ($class->isInterface() ? 'interface' : 'class'),
        'final' => $class->isEnum() || $class->isFinal(),
        'abstract' => $class->isAbstract(),
        'parent' => $parent === false ? null : $parent->getName(),
        'interfaces' => $interfaces,
        'methods' => $methods,
        'properties' => $properties,
        'cases' => $cases,
    ];
}

/** @return array{constants: array<string, mixed>, functions: array<string, mixed>, classes: array<string, mixed>} */
function perfidious_contract_snapshot(bool $extensionLoaded): array
{
    if ($extensionLoaded) {
        $constants = (new ReflectionExtension('perfidious'))->getConstants();
    } else {
        $groups = get_defined_constants(true);
        $constants = $groups['user'] ?? [];
    }

    $constantContract = [];
    foreach ($constants as $name => $value) {
        if (!perfidious_contract_is_public_symbol($name, 'constant')) {
            continue;
        }

        $constantContract[$name] = $name === 'Perfidious\\DEBUG' ? get_debug_type($value) : $value;
    }
    ksort($constantContract);

    $functionContract = [];
    foreach (perfidious_contract_functions($extensionLoaded) as $name => $function) {
        if (!perfidious_contract_is_public_symbol($name, 'function')) {
            continue;
        }

        $functionContract[$name] = perfidious_contract_callable($function);
    }
    ksort($functionContract);

    $classContract = [];
    foreach (perfidious_contract_classes($extensionLoaded) as $name => $class) {
        if (!perfidious_contract_is_public_symbol($name, 'class')) {
            continue;
        }

        $classContract[$name] = perfidious_contract_class($class);
    }
    ksort($classContract);

    return [
        'constants' => $constantContract,
        'functions' => $functionContract,
        'classes' => $classContract,
    ];
}

/** @return list<string> */
function perfidious_contract_diff(mixed $runtime, mixed $stub, string $path = ''): array
{
    if (!is_array($runtime) || !is_array($stub)) {
        if ($runtime === $stub) {
            return [];
        }

        return [sprintf('%s: runtime=%s stub=%s', $path, var_export($runtime, true), var_export($stub, true))];
    }

    $differences = [];
    foreach (array_unique(array_merge(array_keys($runtime), array_keys($stub))) as $key) {
        $childPath = $path === '' ? (string) $key : $path . '.' . $key;
        if (!array_key_exists($key, $runtime)) {
            $differences[] = $childPath . ': missing from runtime';
            continue;
        }
        if (!array_key_exists($key, $stub)) {
            $differences[] = $childPath . ': missing from stub';
            continue;
        }

        array_push($differences, ...perfidious_contract_diff($runtime[$key], $stub[$key], $childPath));
    }

    return $differences;
}
