#!/usr/bin/env groovy

@Library('caf-continuous-integration') _

// Default CMake flags for release builds.
defaultReleaseBuildFlags = [
    'CAF_ENABLE_RUNTIME_CHECKS:BOOL=ON',
]

// Default CMake flags for debug builds.
defaultDebugBuildFlags = defaultReleaseBuildFlags + [
    'CAF_SANITIZERS:STRING=address,undefined',
    'CAF_LOG_LEVEL:STRING=TRACE',
    'CAF_ENABLE_ACTOR_PROFILER:BOOL=ON',
]

// Configures the behavior of our stages.
config = [
    // GitHub path to repository.
    repository: 'actor-framework/actor-framework',
    // List of enabled checks for email notifications.
    checks: [
        'build',
        'style',
        'tests',
        'coverage',
    ],
    // Our build matrix. Keys are the operating system labels and values are build configurations.
    buildMatrix: [
        // Various Linux builds for debug and release.
        ['debian-8', [
            builds: ['debug', 'release'],
            tools: ['clang-4'],
        ]],
        ['centos-6', [
            builds: ['debug', 'release'],
            tools: ['gcc-7'],
        ]],
        ['centos-7', [
            builds: ['debug', 'release'],
            tools: ['gcc-7'],
        ]],
        ['ubuntu-16.04', [
            builds: ['debug', 'release'],
            tools: ['clang-4'],
        ]],
        ['ubuntu-18.04', [
            builds: ['debug', 'release'],
            tools: ['gcc-7'],
        ]],
        // On Fedora 28, our debug build also produces the coverage report.
        ['fedora-28', [
            builds: ['debug'],
            tools: ['gcc-8'],
            extraSteps: ['coverageReport'],
            extraFlags: ['BUILD_SHARED_LIBS:BOOL=OFF'],
        ]],
        ['fedora-28', [
            builds: ['release'],
            tools: ['gcc-8'],
        ]],
        // Other UNIX systems.
        ['macOS', [
            builds: ['debug', 'release'],
            tools: ['clang'],
        ]],
        ['FreeBSD', [
            builds: ['debug', 'release'],
            tools: ['clang'],
        ]],
        // Non-UNIX systems.
        ['Windows', [
            // TODO: debug build currently broken
            //builds: ['debug', 'release'],
            builds: ['release'],
            tools: ['msvc'],
        ]],
    ],
    // Platform-specific environment settings.
    buildEnvironments: [
        nop: [], // Dummy value for getting the proper types.
    ],
    // Default CMake flags by build type.
    defaultBuildFlags: [
        debug: defaultDebugBuildFlags,
        release: defaultReleaseBuildFlags,
    ],
    // CMake flags by OS and build type to override defaults for individual builds.
    buildFlags: [
        macOS: [
            debug: defaultDebugBuildFlags + [
                'OPENSSL_ROOT_DIR=/usr/local/opt/openssl',
                'OPENSSL_INCLUDE_DIR=/usr/local/opt/openssl/include',
            ],
            release: defaultReleaseBuildFlags + [
                'OPENSSL_ROOT_DIR=/usr/local/opt/openssl',
                'OPENSSL_INCLUDE_DIR=/usr/local/opt/openssl/include',
            ],
        ],
        Windows: [
            debug: defaultDebugBuildFlags + [
              'CAF_ENABLE_OPENSSL_MODULE:BOOL=OFF',
            ],
            release: defaultReleaseBuildFlags + [
              'CAF_ENABLE_OPENSSL_MODULE:BOOL=OFF',
            ],
        ],
    ],
    // Configures what binary the coverage report uses and what paths to exclude.
    coverage: [
        binaries: [
          'build/libcaf_core/caf-core-test',
          'build/libcaf_io/caf-io-test',
        ],
        relativeExcludePaths: [
            'examples',
            'tools',
            'libcaf_test',
            'libcaf_core/test',
            'libcaf_io/test',
            'libcaf_openssl/test',
        ],
    ],
]

// Declarative pipeline for triggering all stages.
pipeline {
    options {
        // Store no more than 50 logs and 10 artifacts.
        buildDiscarder(logRotator(numToKeepStr: '50', artifactNumToKeepStr: '3'))
    }
    agent {
        label 'master'
    }
    environment {
        PrettyJobBaseName = env.JOB_BASE_NAME.replace('%2F', '/')
        PrettyJobName = "CAF/$PrettyJobBaseName #${env.BUILD_NUMBER}"
        ASAN_OPTIONS = 'detect_leaks=0'
    }
    stages {
        stage('Checkout') {
            steps {
                getSources(config)
            }
        }
        stage('Lint') {
            agent { label 'clang-format' }
            steps {
                runClangFormat(config)
            }
        }
        stage('Check Consistency') {
            agent { label 'unix' }
            steps {
                deleteDir()
                unstash('sources')
                dir('sources') {
                    cmakeBuild([
                        buildDir: 'build',
                        installation: 'cmake in search path',
                        sourceDir: '.',
                        cmakeArgs: '-DCAF_ENABLE_IO_MODULE:BOOL=OFF ' +
                                   '-DCAF_ENABLE_UTILITY_TARGETS:BOOL=ON',
                        steps: [[
                            args: '--target consistency-check',
                            withCmake: true,
                        ]],
                    ])
                }
            }
        }
        stage('Build') {
            steps {
                buildParallel(config, PrettyJobBaseName)
            }
        }
        stage('Notify') {
            steps {
                collectResults(config, PrettyJobName)
            }
        }
    }
    post {
        failure {
            emailext(
                subject: "$PrettyJobName: " + config['checks'].collect{ "⛔️ ${it}" }.join(', '),
                recipientProviders: [culprits(), developers(), requestor(), upstreamDevelopers()],
                attachLog: true,
                compressLog: true,
                body: "Check console output at ${env.BUILD_URL} or see attached log.\n",
            )
            notifyAllChecks(config, 'failure', 'Failed due to earlier error')
        }
    }
}
