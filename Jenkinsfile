#!/usr/bin/env groovy

@Library('caf-continuous-integration') _

// Default CMake flags for release builds.
defaultReleaseBuildFlags = [
    'CAF_ENABLE_RUNTIME_CHECKS:BOOL=yes',
    'CAF_NO_OPENCL:BOOL=yes',
    'CAF_INSTALL_UNIT_TESTS:BOOL=yes',
]

// Default CMake flags for debug builds.
defaultDebugBuildFlags = defaultReleaseBuildFlags + [
    'CAF_ENABLE_ADDRESS_SANITIZER:BOOL=yes',
    'CAF_LOG_LEVEL:STRING=TRACE',
    'CAF_ENABLE_ACTOR_PROFILER:BOOL=yes',
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
    ],
    // Configures what binary the coverage report uses and what paths to exclude.
    coverage: [
        binaries: [
          'build/bin/caf-core-test',
          'build/bin/caf-io-test',
        ],
        relativeExcludePaths: [
            'examples',
            'tools',
            'libcaf_test',
            'libcaf_core/test',
            'libcaf_io/test',
            'libcaf_openssl/test',
            'libcaf_opencl',
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
        stage('Build') {
            steps {
                buildParallel(config, PrettyJobBaseName)
            }
        }
        stage('Documentation') {
            agent { label 'pandoc' }
            steps {
                deleteDir()
                unstash('sources')
                dir('sources') {
                    // Configure and build.
                    cmakeBuild([
                        buildDir: 'build',
                        installation: 'cmake in search path',
                        sourceDir: '.',
                        cmakeArgs: '-DCAF_BUILD_TEX_MANUAL=yes',
                        steps: [[
                            args: '--target doc',
                            withCmake: true,
                        ]],
                    ])
                    sshagent(['84d71a75-cbb6-489a-8f4c-d0e2793201e9']) {
                        sh """
                            if [ "${env.GIT_BRANCH}" = "master" ]; then
                                rsync -e "ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null" -r -z --delete build/doc/html/ www.inet.haw-hamburg.de:/users/www/www.actor-framework.org/html/doc
                                scp -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null build/doc/manual.pdf www.inet.haw-hamburg.de:/users/www/www.actor-framework.org/html/pdf/manual.pdf
                            fi
                        """
                    }
                }
                dir('read-the-docs') {
                    git([
                        credentialsId: '9b054212-9bb4-41fd-ad8e-b7d47495303f',
                        url: 'git@github.com:actor-framework/read-the-docs.git',
                    ])
                    sh """
                        if [ "${env.GIT_BRANCH}" = "master" ]; then
                            cp ../sources/build/doc/rst/* .
                            if [ -n "\$(git status --porcelain)" ]; then
                                git add .
                                git commit -m "Update Manual"
                                git push --set-upstream origin master
                                if [ -z "\$(grep 'exp.sha' ../sources/release.txt)" ] ; then
                                    git tag \$(cat ../sources/release.txt)
                                    git push origin \$(cat ../sources/release.txt)
                                fi
                            fi
                        fi
                    """
                }
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
