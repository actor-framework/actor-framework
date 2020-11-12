#!/usr/bin/env groovy

@Library('caf-continuous-integration') _

// Configures the behavior of our stages.
config = [
    // Version dependency for the caf-continuous-integration library.
    ciLibVersion: 1.0,
    // GitHub path to repository.
    repository: 'actor-framework/actor-framework',
    // List of enabled checks for email notifications.
    checks: [
        'build',
        'style',
        'tests',
        // 'coverage', TODO: fix kcov setup
    ],
    // Default CMake flags by build type.
    buildFlags: [
        'CAF_ENABLE_RUNTIME_CHECKS:BOOL=ON',
        'CAF_ENABLE_ACTOR_PROFILER:BOOL=ON',
    ],
    extraDebugFlags: [
        'CAF_LOG_LEVEL:STRING=TRACE',
    ],
    // Our build matrix. Keys are the operating system labels and values are build configurations.
    buildMatrix: [
        // Various Linux builds.
        ['centos-7', [
            numCores: 4,
            tags: ['docker'],
            builds: ['release'],
        ]],
        ['centos-8', [
            numCores: 4,
            tags: ['docker'],
            builds: ['release'],
        ]],
        ['debian-9', [
            numCores: 4,
            tags: ['docker'],
            builds: ['release'],
        ]],
        ['debian-10', [
            numCores: 4,
            tags: ['docker'],
            builds: ['release'],
        ]],
        ['ubuntu-16.04', [
            numCores: 4,
            tags: ['docker'],
            builds: ['release'],
        ]],
        ['ubuntu-18.04', [
            numCores: 4,
            tags: ['docker'],
            builds: ['release'],
        ]],
        ['ubuntu-20.04', [
            numCores: 4,
            tags: ['docker'],
            builds: ['release'],
        ]],
        ['fedora-31', [
            numCores: 4,
            tags: ['docker'],
            builds: ['release'],
        ]],
        ['fedora-32', [
            numCores: 4,
            tags: ['docker'],
            builds: ['release'],
        ]],
        // One extra debug build with exceptions disabled.
        ['centos-7', [
            numCores: 4,
            tags: ['docker'],
            builds: ['debug'],
            extraBuildFlags: [
                'CAF_ENABLE_EXCEPTIONS:BOOL=OFF',
                'CMAKE_CXX_FLAGS:STRING=-fno-exceptions',
            ],
        ]],
        // One extra debug build for leak checking.
        ['fedora-32', [
            numCores: 4,
            tags: ['docker', 'LeakSanitizer'],
            builds: ['debug'],
            extraBuildFlags: [
                'CAF_SANITIZERS:STRING=address',
            ],
            extraBuildEnv: [
                'ASAN_OPTIONS=detect_leaks=1',
            ],
        ]],
        // One extra debug build with static libraries.
        ['debian-10', [
            numCores: 4,
            tags: ['docker'],
            builds: ['debug'],
            extraBuildFlags: [
                'BUILD_SHARED_LIBS:BOOL=OFF',
            ],
        ]],
        // Other UNIX systems.
        ['macOS', [
            numCores: 4,
            builds: ['debug', 'release'],
            extraBuildFlags: [
                'OPENSSL_ROOT_DIR:PATH=/usr/local/opt/openssl',
                'OPENSSL_INCLUDE_DIR:PATH=/usr/local/opt/openssl/include',
            ],
            extraDebugBuildFlags: [
                'CAF_SANITIZERS:STRING=address',
            ],
        ]],
        ['FreeBSD', [
            numCores: 4,
            builds: ['debug', 'release'],
            extraBuildFlags: [
                'CAF_SANITIZERS:STRING=address',
            ],
        ]],
        // Non-UNIX systems.
        ['Windows', [
            numCores: 4,
            // TODO: debug build currently broken
            //builds: ['debug', 'release'],
            builds: ['release'],
            extraBuildFlags: [
                'CAF_ENABLE_OPENSSL_MODULE:BOOL=OFF'
            ],
        ]],
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
                buildParallel(config)
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
