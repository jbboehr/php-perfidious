<?php
/**
 * Copyright (c) anno Domini nostri Jesu Christi MMXXIV John Boehr & contributors
 *
 * SPDX-License-Identifier: AGPL-3.0-only WITH romic-exception
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License version 3,
 * as published by the Free Software Foundation, together with the Romic
 * Exception (an additional permission under section 7 of that license).
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * and the Romic Exception along with this program.  If not, see
 * <http://www.gnu.org/licenses/> and the LICENSE_EXCEPTION file.
 */

namespace Perfidious;

/** @var bool Whether the extension was built with its debug-only test hooks. */
const DEBUG = false;

const VERSION = "0.2.0";

const OVERFLOW_THROW = 0;
const OVERFLOW_WARN = 1;
const OVERFLOW_SATURATE = 2;
const OVERFLOW_WRAP = 3;

const MOTD = "Think not that I am come to send peace on earth: I came not to send peace, but a sword. Matthew 10:34";

interface ExceptionInterface
{
}

final class IOException extends \RuntimeException implements ExceptionInterface
{
}

final class OverflowException extends \OverflowException implements ExceptionInterface
{
}

final class PmuNotFoundException extends \InvalidArgumentException implements ExceptionInterface
{
}

final class PmuEventNotFoundException extends \InvalidArgumentException implements ExceptionInterface
{
}

final class UnsupportedMetricException extends \RuntimeException implements ExceptionInterface
{
    private function __construct()
    {
    }

    public readonly Scope $scope;

    /** @var non-empty-list<Metric> */
    public readonly array $unsupportedMetrics;
}

enum Scope: string
{
    case CurrentProcess = 'current-process';
    case CurrentThread = 'current-thread';
}

enum Metric: string
{
    case CpuTime = 'cpu-time';
    case PageFaults = 'page-faults';
    case ContextSwitches = 'context-switches';
    case CpuCycles = 'cpu-cycles';
    case Instructions = 'instructions';
}

final class Sampler
{
    private function __construct()
    {
    }

    /** @param non-empty-list<Metric> $metrics */
    public static function open(array $metrics, Scope $scope = Scope::CurrentProcess): self
    {
    }

    /** @return non-empty-list<Metric> */
    public function metrics(): array
    {
    }

    public function read(): Sample
    {
    }

    public function close(): void
    {
    }
}

final class Sample
{
    private function __construct()
    {
    }

    public function value(Metric $metric): int
    {
    }

    public function since(self $earlier): SampleDelta
    {
    }
}

final class SampleDelta
{
    private function __construct()
    {
    }

    public readonly int $elapsedTimeNs;

    public function value(Metric $metric): int
    {
    }
}

/**
 * @phpstan-template T of list<string>
 */
final class ReadResult
{
    public readonly int $timeEnabled;
    public readonly int $timeRunning;
    /**
     * @var array<string, int>
     * @phpstan-var array<value-of<T>, int>
     */
    public readonly array $values;
}
