#!/usr/bin/env groovy

@Library('caf-continuous-integration') _

// Default CMake flags for release builds.
defaultReleaseBuildFlags = [
    'CAF_ENABLE_RUNTIME_CHECKS:BOOL=ON',
    'CAF_ENABLE_ACTOR_PROFILER:BOOL=ON',
]

// Default CMake flags for debug builds.
defaultDebugBuildFlags = defaultReleaseBuildFlags + [
    'CAF_LOG_LEVEL:STRING=TRACE',
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
        // 'coverage', TODO: fix kcov setup
    ],
    // Our build matrix. Keys are the operating system labels and values are build configurations.
    buildMatrix: [
        // Various Linux builds for debug and release.
        ['centos-6', [
            numCores: 4,
            tags: ['docker'],
            builds: ['debug', 'release'],
            extraDebugFlags: ['CAF_SANITIZERS:STRING=address,undefined'],
        ]],
        ['centos-7', [
            numCores: 4,
            tags: ['docker'],
            builds: ['debug', 'release'],
            extraDebugFlags: ['CAF_SANITIZERS:STRING=address,undefined'],
        ]],
        ['centos-8', [
            numCores: 4,
            tags: ['docker'],
            builds: ['debug', 'release'],
            extraDebugFlags: ['CAF_SANITIZERS:STRING=address,undefined'],
        ]],
        ['debian-9', [
            numCores: 4,
            tags: ['docker'],
            builds: ['debug', 'release'],
        ]],
        ['debian-10', [
            numCores: 4,
            tags: ['docker'],
            builds: ['debug', 'release'],
        ]],
        ['ubuntu-16.04', [
            numCores: 4,
            tags: ['docker'],
            builds: ['debug', 'release'],
        ]],
        ['ubuntu-18.04', [
            numCores: 4,
            tags: ['docker'],
            builds: ['debug', 'release'],
        ]],
        ['ubuntu-20.04', [
            numCores: 4,
            tags: ['docker'],
            builds: ['debug', 'release'],
        ]],
        ['fedora-30', [
            numCores: 4,
            tags: ['docker'],
            builds: ['debug', 'release'],
        ]],
        ['fedora-31', [
            numCores: 4,
            tags: ['docker'],
            builds: ['debug', 'release'],
        ]],
        ['fedora-32', [
            numCores: 4,
            tags: ['docker'],
            builds: ['debug', 'release'],
        ]],
        // One extra debug build with exceptions disabled.
        ['centos-7', [
            numCores: 4,
            tags: ['docker'],
            builds: ['debug'],
            extraDebugFlags: [
                'CAF_ENABLE_EXCEPTIONS:BOOL=OFF',
                'CMAKE_CXX_FLAGS:STRING=-fno-exceptions',
            ],
        ]],
        // One extra debug build with static libraries.
        ['debian-10', [
            numCores: 4,
            tags: ['docker'],
            builds: ['debug', 'release'],
            extraDebugFlags: [
                'BUILD_SHARED_LIBS:BOOL=OFF',
            ],
        ]],
        // Other UNIX systems.
        ['macOS', [
            numCores: 4,
            builds: ['debug', 'release'],
            extraFlags: [
                'OPENSSL_ROOT_DIR=/usr/local/opt/openssl',
                'OPENSSL_INCLUDE_DIR=/usr/local/opt/openssl/include',
            ],
            extraDebugFlags: ['CAF_SANITIZERS:STRING=address,undefined'],
        ]],
        ['FreeBSD', [
            numCores: 4,
            builds: ['debug', 'release'],
            extraDebugFlags: ['CAF_SANITIZERS:STRING=address,undefined'],
        ]],
        // Non-UNIX systems.
        ['Windows', [
            numCores: 4,
            // TODO: debug build currently broken
            //builds: ['debug', 'release'],
            builds: ['release'],
            extraFlags: ['CAF_ENABLE_OPENSSL_MODULE:BOOL=OFF'],
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
        nop: [],
    ],
    // Default CMake flags by build type.
    defaultBuildFlags: [
        debug: defaultDebugBuildFlags,
        release: defaultReleaseBuildFlags,
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
